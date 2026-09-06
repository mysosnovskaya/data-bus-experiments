// ============================================================================
// TbbRunnerNatural — ЕСТЕСТВЕННОЕ распараллеливание средствами oneTBB.
//
// Программе передаётся только ЗАДАЧА: список работ и частичный порядок (DEPS).
// Ни порядка выполнения, ни числа ядер на работу, ни числа кусков снаружи не
// задаётся — всё это решает рантайм. Так этот код и был бы написан в реальной
// практике.
//
// Два уровня параллелизма, оба отданы TBB:
//   1) МЕЖДУ работами — flow graph: узел на работу, рёбра из DEPS. Какие из
//      готовых работ идут одновременно и в каком порядке, выбирает планировщик.
//   2) ВНУТРИ работы — parallel_for по ДИАПАЗОНУ ДАННЫХ. Работа это 100 итераций
//      векторной операции над массивом; массив выделяется ОДИН раз, а на сколько
//      частей его разрезать и сколько потоков привлечь, решает auto_partitioner.
//
// Почему по диапазону данных, а не нарезкой работы на подработы: подработы
// потребовали бы отдельного выделения памяти на каждый кусок и фиксированного
// числа кусков снаружи — и то, и другое искусственно. Здесь единица дробления
// выбирается рантаймом свободно, вплоть до отдельных элементов.
//
// Каждая итерация запускает СВОЙ parallel_for, поэтому решение о параллельности
// пересматривается 100 раз за работу: освободившееся ядро подключается со
// следующей итерации, а не ждёт конца работы.
//
// Привязка к ядрам оставлена только ради чистоты измерения: все счётные потоки
// держатся на четырёх ядрах ОДНОГО сокета, иначе работы разъезжаются по двум
// шинам памяти и конкуренция за шину, которую мы и изучаем, исчезает.
//
// ЛОГИ: по каждой работе печатается, как она распараллелилась — сколько потоков
// участвовало, какую долю работы сделал каждый и какой получился фактический
// параллелизм (суммарное время потоков, делённое на время работы).
// ============================================================================

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <sched.h>
#include <string>
#include <thread>
#include <vector>

#include <tbb/tbb.h>
#include <tbb/flow_graph.h>

#include "../common/JobsParallel.hpp"

using namespace std;
using namespace std::chrono;
using namespace tbb::flow;

// --- топология: 4 счётных ядра одного сокета + ядро для главного потока ---
static const int N_CORES = 4;
static const int CORE_NUMBERS[N_CORES] = {0, 1, 4, 5};
static const int ORCHESTRATOR_CORE = 6;
static const int ITERATION_COUNT = 1;          // прогонов каждого примера

// ---------------------------------------------------------------------------
// Учёт распараллеливания одной работы: кто сколько сделал.
// Индекс слота арены = «номер потока» внутри арены, 0..N_CORES-1.
// ---------------------------------------------------------------------------
struct ParallelismStats {
    atomic<long long> nanosBySlot[N_CORES];
    atomic<long long> chunksBySlot[N_CORES];
    atomic<long long> elementsBySlot[N_CORES];

    ParallelismStats() {
        for (int i = 0; i < N_CORES; i++) {
            nanosBySlot[i] = 0;
            chunksBySlot[i] = 0;
            elementsBySlot[i] = 0;
        }
    }
};

static void pinThisThreadTo(int physCore) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(physCore, &set);
    if (sched_setaffinity(0, sizeof(set), &set) < 0) {
        printf("!!! ПИННИНГ НЕ УДАЛСЯ: ядро %d, errno=%d (%s)\n", physCore, errno, strerror(errno));
    }
}

// ---------------------------------------------------------------------------
// Наблюдатель арены: поток, занявший слот i, садится на CORE_NUMBERS[i].
// Это штатный способ привязки в TBB — по слоту, а не по какому-то своему пулу.
// ---------------------------------------------------------------------------
class PinningObserver : public tbb::task_scheduler_observer {
public:
    explicit PinningObserver(tbb::task_arena& arena) : tbb::task_scheduler_observer(arena) {
        observe(true);
    }

    void on_scheduler_entry(bool) override {
        int slot = tbb::this_task_arena::current_thread_index();
        if (slot >= 0 && slot < N_CORES) {
            pinThisThreadTo(CORE_NUMBERS[slot]);
        }
    }
};

// ---------------------------------------------------------------------------
// Задача: работы и частичный порядок. Ничего больше.
// ---------------------------------------------------------------------------
struct Project {
    string name;
    vector<pair<string, int>> jobs;          // тип и размер
    vector<pair<int, int>> deps;             // ребро предшественник -> преемник
};

static Job* makeJob(const string& type, int size) {
    if (type == "ERF") return MklErfJob::create(size);
    if (type == "TGAMMA") return MklTgammaJob::create(size);
    if (type == "POW") return MklPowJob::create(size);
    if (type == "SQRTX") return MklSqrtJob::create(size);
    if (type == "COPY") return MklCopyJob::create(size);
    if (type == "SUM") return MklSumJob::create(size);
    if (type == "XPY") return MklXpyJob::create(size);
    if (type == "DOT") return MklDotJob::create(size);
    throw runtime_error("Неизвестный тип работы: " + type);
}

static Project parseProject(const string& path, const string& name) {
    ifstream in(path);
    if (!in) throw runtime_error("Не открывается файл: " + path);

    Project project;
    project.name = name;
    string tag;
    int count = 0;

    while (in >> tag) {
        if (tag == "JOBS") {
            in >> count;
            project.jobs.assign(count, {});
            for (int i = 0; i < count; i++) {
                int id, size;
                string type;
                in >> id >> type >> size;
                project.jobs[id] = {type, size};
            }
        } else if (tag == "DEPS") {
            in >> count;
            for (int i = 0; i < count; i++) {
                int from, to;
                in >> from >> to;
                project.deps.push_back({from, to});
            }
        } else {
            // секции ORDER/MODES игнорируем: здесь всё решает TBB
            string rest;
            getline(in, rest);
        }
    }
    return project;
}

// ---------------------------------------------------------------------------
// Выполнение одной работы. Массив выделяется один раз; каждая из 100 итераций
// запускается через parallel_for по диапазону данных, и рантайм сам решает,
// на сколько частей его разбить и сколько потоков привлечь.
// ---------------------------------------------------------------------------
static void runJobNatural(const string& type, int size, int jobId) {
    long long startedAt = (long long) duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    printf("job %d_%s_%d started %lld\n", jobId, type.c_str(), size, startedAt);

    Job* job = makeJob(type, size);
    long scaledSize = job->getScaledSize();

    ParallelismStats stats;
    auto wallStart = steady_clock::now();

    for (int iteration = 0; iteration < Job::ITERATION_COUNT; iteration++) {
        tbb::parallel_for(tbb::blocked_range<long>(0, scaledSize),
            [&job, &stats, iteration](const tbb::blocked_range<long>& range) {
                int slot = tbb::this_task_arena::current_thread_index();
                if (slot < 0 || slot >= N_CORES) slot = 0;

                auto t0 = steady_clock::now();
                job->executeRange(range.begin(), range.end(), iteration);
                auto t1 = steady_clock::now();

                stats.nanosBySlot[slot] += duration_cast<nanoseconds>(t1 - t0).count();
                stats.chunksBySlot[slot] += 1;
                stats.elementsBySlot[slot] += range.end() - range.begin();
            });
        // партиционер не указан — используется auto_partitioner, то есть
        // гранулярность выбирает сам рантайм
    }

    double wallMs = duration<double, milli>(steady_clock::now() - wallStart).count();
    delete job;

    long long totalNanos = 0;
    long long totalChunks = 0;
    int threadsUsed = 0;
    for (int i = 0; i < N_CORES; i++) {
        totalNanos += stats.nanosBySlot[i];
        totalChunks += stats.chunksBySlot[i];
        if (stats.nanosBySlot[i] > 0) threadsUsed++;
    }
    double parallelism = wallMs > 0 ? (double) totalNanos / 1e6 / wallMs : 1.0;

    // как именно распараллелилась работа
    printf("job %d_%s_%d done %lld | время %.0f ms | параллелизм %.2f | потоков %d | частей %lld | доли:",
           jobId, type.c_str(), size,
           (long long) duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count(),
           wallMs, parallelism, threadsUsed, totalChunks);
    for (int i = 0; i < N_CORES; i++) {
        double share = totalNanos > 0 ? (double) stats.nanosBySlot[i] / totalNanos * 100.0 : 0.0;
        printf(" слот%d(ядро%d)=%.0f%%", i, CORE_NUMBERS[i], share);
    }
    printf("\n");
    fflush(stdout);
}

// ---------------------------------------------------------------------------
// Граф работ: узел на работу, рёбра из DEPS. Порядок выбирает планировщик —
// приоритетов и подсказок не задаётся.
// ---------------------------------------------------------------------------
static double runGraph(const Project& project, tbb::task_arena& arena) {
    int jobCount = (int) project.jobs.size();

    graph g;
    broadcast_node<continue_msg> start(g);
    vector<continue_node<continue_msg>*> nodes(jobCount);

    for (int i = 0; i < jobCount; i++) {
        const string& type = project.jobs[i].first;
        int size = project.jobs[i].second;
        nodes[i] = new continue_node<continue_msg>(g, [type, size, i](const continue_msg&) {
            runJobNatural(type, size, i);
        });
    }

    vector<bool> hasPredecessor(jobCount, false);
    for (auto& edge : project.deps) {
        make_edge(*nodes[edge.first], *nodes[edge.second]);
        hasPredecessor[edge.second] = true;
    }
    for (int i = 0; i < jobCount; i++) {
        if (!hasPredecessor[i]) make_edge(start, *nodes[i]);
    }

    auto t0 = steady_clock::now();
    promise<void> done;
    future<void> future = done.get_future();
    arena.enqueue([&] {
        start.try_put(continue_msg());
        g.wait_for_all();
        done.set_value();
    });
    future.wait();
    auto t1 = steady_clock::now();

    for (int i = 0; i < jobCount; i++) delete nodes[i];
    return duration<double, milli>(t1 - t0).count();
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "usage: " << argv[0] << " <папка_с_проектами> <файл_результатов>\n"
             << "  На вход идут только работы и частичный порядок; как их\n"
             << "  распараллелить, полностью решает oneTBB.\n";
        return 1;
    }
    string projectsDir = argv[1];
    string outputPath = argv[2];

    // Главный поток и счётные ядра. Маска процесса шире на одно ядро, чтобы
    // главный поток не отнимал счётное ядро, пока ждёт завершения графа.
    cpu_set_t processMask;
    CPU_ZERO(&processMask);
    for (int i = 0; i < N_CORES; i++) CPU_SET(CORE_NUMBERS[i], &processMask);
    CPU_SET(ORCHESTRATOR_CORE, &processMask);
    if (sched_setaffinity(0, sizeof(processMask), &processMask) < 0) {
        cerr << "Не удалось задать маску процесса\n";
    }
    pinThisThreadTo(ORCHESTRATOR_CORE);

    // Все N_CORES слотов арены отданы рабочим потокам (второй аргумент 0):
    // по умолчанию один слот резервируется под внешний поток, а наш главный
    // поток в арену не входит — он ждёт на future.
    tbb::global_control control(tbb::global_control::max_allowed_parallelism, N_CORES + 1);
    tbb::task_arena arena(N_CORES, 0);
    PinningObserver observer(arena);

    printf("BUILD: естественное распараллеливание oneTBB (parallel_for по данным, auto_partitioner)\n");
    printf("Счётных ядер: %d, слотов арены: %d\n", N_CORES, arena.max_concurrency());
    fflush(stdout);

    vector<string> names;
    for (const auto& entry : std::filesystem::directory_iterator(projectsDir)) {
        if (entry.path().extension() == ".txt") {
            names.push_back(entry.path().filename().string());
        }
    }
    sort(names.begin(), names.end());
    if (names.empty()) {
        cerr << "нет *.txt в " << projectsDir << "\n";
        return 2;
    }

    ofstream out(outputPath);
    out << "# project  avg_makespan_ms (естественный oneTBB, " << ITERATION_COUNT << " прогонов)\n";

    for (const string& name : names) {
        Project project = parseProject(projectsDir + "/" + name, name);
        cout << "=== Выполняется проект: " << name << " (" << project.jobs.size()
             << " работ, " << project.deps.size() << " зависимостей; распараллеливание выбирает TBB) ===" << endl;

        double sum = 0.0;
        for (int iteration = 0; iteration < ITERATION_COUNT; iteration++) {
            double ms = runGraph(project, arena);
            sum += ms;
            cout << "  итерация " << iteration << ": " << (long) ms << " ms" << endl;
        }
        long average = (long) (sum / ITERATION_COUNT);
        cout << "  среднее по " << ITERATION_COUNT << " итерациям: " << average << " ms" << endl;
        out << name << "  " << average << "\n";
        out.flush();
    }
    cout << "Готово. Результаты: " << outputPath << endl;
    return 0;
}