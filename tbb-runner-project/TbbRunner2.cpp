#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <pthread.h>
#include <sched.h>

#include "../common/Jobs.hpp"
#include <tbb/tbb.h>
#include <tbb/flow_graph.h>

using namespace std;
using namespace std::chrono;

static const vector<int> CORE_NUMBERS = {0, 1, 4, 5};
static const int N_CORES = 4;
static const int ITERATION_COUNT = 1;   // было 5

// Пары ядер (die), делящие L2-кэш: {0,4} и {1,5}.
static const set<int> DIE_A_CORES = {0, 4};
static bool inDieA(int c) { return DIE_A_CORES.count(c) > 0; }

static int coreIndexOf(int physCore) {
    for (int i = 0; i < (int)CORE_NUMBERS.size(); i++)
        if (CORE_NUMBERS[i] == physCore) return i;
    return -1;
}

// Прибить ВЫЗЫВАЮЩИЙ поток к конкретному физическому ядру.
static void pinThisThreadTo(int physCore) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(physCore, &cs);
    if (sched_setaffinity(0, sizeof(cs), &cs) < 0)
        cerr << "Unable to set affinity to core " << physCore << endl;
}

// ---------------------------------------------------------------------------
// Аффинити для потоков главной арены (базовый прогон без ORDER: oneTBB сам
// выбирает порядок и ядро). Каждый поток арены прибит к своему ядру по индексу.
// ---------------------------------------------------------------------------
class PinningObserver : public tbb::task_scheduler_observer {
public:
    PinningObserver(tbb::task_arena& a) : tbb::task_scheduler_observer(a) { observe(true); }
    void on_scheduler_entry(bool) override {
        int idx = tbb::this_task_arena::current_thread_index();
        if (idx >= 0 && idx < (int)CORE_NUMBERS.size())
            pinThisThreadTo(CORE_NUMBERS[idx]);
    }
};

// ---------------------------------------------------------------------------
// Фабрика работ (коды в Jobs.hpp). size — уже итоговый размер подзадачи.
// ---------------------------------------------------------------------------
static Job* makeJob(const string& type, int size) {
    if (type == "COPY")        return MklCopyJob::create(size);
    else if (type == "SUM")    return MklSumJob::create(size);
    else if (type == "DOT")    return MklDotJob::create(size);
    else if (type == "XPY")    return MklXpyJob::create(size);
    else if (type == "SQRTX")  return MklSqrtJob::create(size);
//    else if (type == "LNX")    return MklLnJob::create(size);
//    else if (type == "EXPX")   return MklExpJob::create(size);
    else if (type == "ERF")    return MklErfJob::create(size);
    else if (type == "TGAMMA") return MklTgammaJob::create(size);
    else if (type == "POW")    return MklPowJob::create(size);
    else                       return MklXpyJob::create(size);
}

// ---------------------------------------------------------------------------
// Исполнение ОДНОЙ работы проекта.
//   cores пусто  -> мода 1: исполняется прямо здесь (на потоке TBB, ядро
//                   выбрал движок). Используется в базовом прогоне без ORDER.
//   cores = k    -> мода k: k подзадач размера size/k, каждая на своём ядре
//                   (поток с аффинити), ждём завершения всех. Для k=1 —
//                   полноразмерная работа на одном заданном ядре.
// ---------------------------------------------------------------------------
static void runJob(const string& type, int size, const vector<int>& cores, int jobId) {
    printf("job %d_%s_%d started %lld\n",
       jobId,
       type.c_str(),
       size,
       static_cast<long long>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()));
    if (cores.empty()) {                          // мода 1, ядро выбирает TBB
        Job* job = makeJob(type, size);
        double tmp = 0.0;
        job->execute(&tmp, false);
        delete job;
        printf("job %d_%s_%d end %lld\n",
            jobId,
            type.c_str(),
            size,
            static_cast<long long>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()));
        return;
    }
    int k = (int)cores.size();                    // мода k (k=1 -> одно ядро, полный size)
    int subSize = size / k;                        // размер подзадачи (как --cores k)
    vector<thread> threads;
    threads.reserve(k);
    for (int c : cores) {
        threads.emplace_back([type, subSize, c]() {
            pinThisThreadTo(c);
            Job* sub = makeJob(type, subSize);
            double tmp = 0.0;
            sub->execute(&tmp, false);
            delete sub;
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    printf("job %d_%s_%d end %lld\n",
        jobId,
        type.c_str(),
        size,
        static_cast<long long>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()));
}

// ---------------------------------------------------------------------------
// Проект. Новый формат:
//   JOBS N / <id> <TYPE> <SIZE>            — работы
//   DEPS M / <pred> <succ>                 — частичный порядок (может быть пуст)
//   [ORDER N / <id> ...]                   — перестановка приоритета (опц.)
//   [MODES N / <id> <k> ...]               — число ядер на работу (опц., по умолч. 1)
// Секции ядер (CORES) больше НЕТ — ядра назначает рантайм.
// ---------------------------------------------------------------------------
struct Project {
    string name;
    vector<pair<string,int>> jobs;   // (type, size) по id
    vector<pair<int,int>> deps;      // (pred, succ)
    vector<int> order;               // перестановка приоритета (пусто = базовый прогон)
    vector<int> modeOf;              // modeOf[id] = число ядер (по умолчанию 1)
};

static Project parseProject(const string& path, const string& name) {
    Project p; p.name = name;
    ifstream in(path);
    string tag; int count;

    in >> tag >> count;              // JOBS N
    int N = count;
    p.jobs.resize(N);
    p.modeOf.assign(N, 1);
    for (int i = 0; i < N; i++) {
        int id; string type; int size;
        in >> id >> type >> size;
        p.jobs[id] = {type, size};
    }

    // Оставшиеся секции (DEPS / ORDER / MODES) читаем по тегам, в любом порядке.
    while (in >> tag >> count) {
        if (tag == "DEPS") {
            for (int i = 0; i < count; i++) { int s, d; in >> s >> d; p.deps.push_back({s, d}); }
        } else if (tag == "ORDER") {
            p.order.resize(count);
            for (int i = 0; i < count; i++) in >> p.order[i];
        } else if (tag == "MODES") {
            for (int i = 0; i < count; i++) { int id, k; in >> id >> k; p.modeOf[id] = k; }
        } else {
            break;                   // неизвестная секция — прекращаем
        }
    }
    return p;
}

// ---------------------------------------------------------------------------
// Выбор ядер под работу из k ядер по политике пар (die):
//   k == 1 -> ядро из пары, где больше свободных (реже делим L2 с соседом);
//   k == 2 -> по одному ядру в каждую пару, если можно, иначе целая пара;
//   k >= 3 -> любые k свободных.
// Выбранные ядра удаляются из freeCores.
// ---------------------------------------------------------------------------
static vector<int> pickCores(int k, set<int>& freeCores) {
    vector<int> a, b;               // свободные ядра пары A ({0,4}) и B ({1,5})
    for (int c : freeCores) (inDieA(c) ? a : b).push_back(c);

    vector<int> chosen;
    if (k == 1) {
        vector<int>& src = (a.size() >= b.size() && !a.empty()) ? a : (!b.empty() ? b : a);
        chosen.push_back(src.front());
    } else if (k == 2) {
        if (!a.empty() && !b.empty()) {           // по одному в каждую пару — без общего L2
            chosen.push_back(a.front());
            chosen.push_back(b.front());
        } else {                                  // целая пара с той стороны, где 2 свободны
            vector<int>& src = (a.size() >= 2) ? a : b;
            chosen.push_back(src[0]);
            chosen.push_back(src[1]);
        }
    } else {                                      // k == 3 или 4 — берём любые k
        for (int c : freeCores) {
            chosen.push_back(c);
            if ((int)chosen.size() == k) break;
        }
    }
    for (int c : chosen) freeCores.erase(c);
    return chosen;
}

// ---------------------------------------------------------------------------
// Прогон ЗАДАННОГО расписания (есть ORDER). Диспетчер = списочный планировщик:
// на каждое освобождение ядер берёт готовую работу с наивысшим приоритетом из
// перестановки, назначает ядра по политике пар и запускает её. Если работа с
// наивысшим приоритетом требует БОЛЬШЕ ядер, чем свободно, — ЖДЁТ их освобождения
// (резервирование), не пропуская её работами низшего приоритета. Для одномодального
// случая (все k=1) ожидание не наступает: свободное ядро всегда берёт работу.
// Возвращает makespan (мс) — реальное время стенки.
// ---------------------------------------------------------------------------
static double runOrdered(const Project& p) {
    int N = (int)p.jobs.size();

    vector<int> predsRemaining(N, 0);
    vector<vector<int>> succ(N);
    for (auto& e : p.deps) { succ[e.first].push_back(e.second); predsRemaining[e.second]++; }

    vector<char> started(N, 0);
    set<int> freeCores(CORE_NUMBERS.begin(), CORE_NUMBERS.end());
    int doneCount = 0, runningCount = 0;

    mutex m;
    condition_variable cv;
    vector<pair<int, vector<int>>> completions;   // (id, освобождённые ядра)
    vector<thread> workers;

    auto t0 = high_resolution_clock::now();

    unique_lock<mutex> lk(m);
    while (doneCount < N) {
        // Запускаем готовые работы по приоритету, с резервированием ядер.
        bool startedAny = true;
        while (startedAny) {
            startedAny = false;
            for (int id : p.order) {
                if (started[id] || predsRemaining[id] > 0) continue;   // не готова
                int k = p.modeOf[id];
                if ((int)freeCores.size() >= k) {
                    vector<int> cores = pickCores(k, freeCores);
                    started[id] = 1;
                    runningCount++;
                    string type = p.jobs[id].first;
                    int size = p.jobs[id].second;
                    workers.emplace_back([&m, &cv, &completions, id, type, size, cores]() {
                        runJob(type, size, cores, id);
                        {
                            lock_guard<mutex> g(m);
                            completions.push_back({id, cores});
                        }
                        cv.notify_one();
                    });
                    startedAny = true;
                    break;                     // пересканировать очередь с начала (по приоритету)
                } else {
                    // Работа с высшим приоритетом не помещается -> ждём ядра (резервирование).
                    break;
                }
            }
        }

        // Ждём хотя бы одно завершение и освобождаем его ядра.
        if (runningCount > 0) {
            cv.wait(lk, [&]{ return !completions.empty(); });
            for (auto& c : completions) {
                doneCount++;
                runningCount--;
                for (int cr : c.second) freeCores.insert(cr);
                for (int s : succ[c.first]) predsRemaining[s]--;
            }
            completions.clear();
        } else if (doneCount < N) {
            cerr << "Тупик диспетчера в проекте " << p.name << ": проверьте ORDER/DEPS\n";
            break;
        }
    }
    lk.unlock();

    for (auto& t : workers) t.join();

    auto t1 = high_resolution_clock::now();
    return duration<double, milli>(t1 - t0).count();
}

// ---------------------------------------------------------------------------
// Базовый прогон БЕЗ ORDER: oneTBB flow graph. Узел = работа; рёбра = DEPS;
// узлы без входящих зависимостей стартуют от node0. Все работы — мода 1
// (ядро выбирает движок), oneTBB сам решает порядок. Это эталон для сравнения.
// ---------------------------------------------------------------------------
static double runOnce(const Project& p, tbb::task_arena& arena) {
    using namespace tbb::flow;
    int N = (int)p.jobs.size();

    graph g;
    broadcast_node<continue_msg> node0(g);
    vector<continue_node<continue_msg>*> nodes(N);
    for (int i = 0; i < N; i++) {
        const string& type = p.jobs[i].first;
        int size = p.jobs[i].second;
        nodes[i] = new continue_node<continue_msg>(
            g, [type, size, i](const continue_msg&) {
                runJob(type, size, {}, i);         // мода 1, ядро выбирает TBB
            });
        make_edge(node0, *nodes[i]);
    }
    for (auto& e : p.deps)
        make_edge(*nodes[e.first], *nodes[e.second]);

    auto t0 = high_resolution_clock::now();
    arena.execute([&]{
        node0.try_put(continue_msg());
        g.wait_for_all();
    });
    auto t1 = high_resolution_clock::now();

    for (int i = 0; i < N; i++) delete nodes[i];
    return duration<double, milli>(t1 - t0).count();
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "usage: " << argv[0] << " <папка_с_проектами> <файл_результатов>\n";
        return 1;
    }
    string projDir = argv[1];
    string outPath = argv[2];

    #ifdef USE_MKL
    mkl_set_num_threads(1);   // каждая (под)задача — однопоточный BLAS/VML
    #endif

    tbb::global_control gc(tbb::global_control::max_allowed_parallelism, N_CORES);
    tbb::task_arena arena(N_CORES);
    PinningObserver observer(arena);

    vector<string> names;
    for (const auto& e : std::filesystem::directory_iterator(projDir))
        if (e.is_regular_file() && e.path().extension() == ".txt")
            names.push_back(e.path().filename().string());
    sort(names.begin(), names.end());
    if (names.empty()) { cerr << "нет *.txt в " << projDir << "\n"; return 2; }

    ofstream out(outPath);
    out << "# project  avg_makespan_ms (oneTBB, " << ITERATION_COUNT << " прогонов)\n";

    for (const string& nm : names) {
        Project p = parseProject(projDir + "/" + nm, nm);
        bool ordered = !p.order.empty();
        cout << "=== Выполняется проект: " << nm
             << " (" << p.jobs.size() << " работ, "
             << (ordered ? "перестановка ORDER" : "базовый прогон oneTBB") << ") ===" << endl;
        double sum = 0.0;
        for (int it = 0; it < ITERATION_COUNT; it++) {
            double ms = ordered ? runOrdered(p) : runOnce(p, arena);
            sum += ms;
            cout << "  итерация " << it << ": " << (long)ms << " ms" << endl;
        }
        long avg = (long)(sum / ITERATION_COUNT);
        cout << "  среднее по " << ITERATION_COUNT << " итерациям: " << avg << " ms" << endl;

        out << nm << " " << avg << "\n";
        out.flush();
    }
    cout << "execution completed -> " << outPath << endl;
    return 0;
}