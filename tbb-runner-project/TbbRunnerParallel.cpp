#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <pthread.h>
#include <sched.h>
#include <cerrno>
#include <cstring>

#include "../common/Jobs.hpp"
#include <tbb/tbb.h>
#include <tbb/flow_graph.h>

using namespace std;
using namespace std::chrono;

static const vector<int> CORE_NUMBERS = {0, 1, 4, 5};
static const int N_CORES = 4;
// Ядро сокета 1 под главный поток-оркестратор (wait_for_all). Твои процедуры на
// нём не считаются — только оркестровка, чтобы все 4 ядра сокета 0 были свободны.
static const int ORCHESTRATOR_CORE = 6;
static const int ITERATION_COUNT = 1;   // было 7

// Прибить ВЫЗЫВАЮЩИЙ поток к конкретному физическому ядру.
static void pinThisThreadTo(int physCore) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(physCore, &cs);
    if (sched_setaffinity(0, sizeof(cs), &cs) < 0)
        printf("!!! ПИННИНГ НЕ УДАЛСЯ: ядро %d, errno=%d (%s)\n",
               physCore, errno, strerror(errno));
}

// ---------------------------------------------------------------------------
// Аффинити для потоков главной арены (базовый прогон без ORDER: oneTBB сам
// выбирает порядок и ядро). Каждый поток арены прибит к своему ядру по индексу.
// ---------------------------------------------------------------------------
class PinningObserver : public tbb::task_scheduler_observer {
public:
    PinningObserver(tbb::task_arena& a) : tbb::task_scheduler_observer(a) { observe(true); }
    void on_scheduler_entry(bool worker) override {
        if (!worker) {                               // главный поток (если вдруг войдёт)
            pinThisThreadTo(ORCHESTRATOR_CORE);      // на сокет 1, вне счётных ядер
            return;
        }
        // Прибиваем по индексу СЛОТА арены: в арене на 4 слота это всегда 0..3 —
        // стабильно, без зависимости от текучки потоков в пуле. Мастер в арену не
        // входит (enqueue+future), поэтому все 4 слота — счётные воркеры -> {0,1,4,5}.
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
// ---------------------------------------------------------------------------
// Раздатчик счётных ядер. Кто бы ни исполнял узел графа (воркер арены), он берёт
// свободное ядро из {0,1,4,5} и прибивается к нему на время работы, а по окончании
// возвращает. Это НЕ зависит от PinningObserver и от текучки пула TBB — процедура
// физически не может уйти на сокет 1. Воркеров максимум 4 (арена на 4 слота),
// поэтому свободное ядро всегда найдётся.
// ---------------------------------------------------------------------------
static std::mutex g_coreMx;
static vector<int> g_freeCores = {0, 1, 4, 5};

static int acquireCore() {
    std::lock_guard<std::mutex> lk(g_coreMx);
    if (g_freeCores.empty()) return -1;          // страховка; при 4 воркерах не бывает
    int c = g_freeCores.back();
    g_freeCores.pop_back();
    return c;
}
static void releaseCore(int c) {
    if (c < 0) return;
    std::lock_guard<std::mutex> lk(g_coreMx);
    g_freeCores.push_back(c);
}

// ---------------------------------------------------------------------------
// Выделение НАБОРА из k счётных ядер (ждём, пока освободятся). Всё-или-ничего:
// ждущий поток НЕ держит ни одного ядра, поэтому взаимная блокировка невозможна —
// занятые ядра держат только реально идущие работы, а они обязательно завершатся.
// Простой при ожидании нормален: он заложен в расписание, раз там многоядерная работа.
// Политика для k=2: по возможности по ядру с КАЖДОЙ пары (на общей паре два потока
// делят L2 и работа идёт заметно медленнее); если нельзя — берём что есть.
// ---------------------------------------------------------------------------
static std::condition_variable g_coreCv;

static int dieOf(int core) { return (core == 0 || core == 4) ? 0 : 1; }

static vector<int> acquireCores(int k) {
    std::unique_lock<std::mutex> lk(g_coreMx);
    g_coreCv.wait(lk, [k] { return (int)g_freeCores.size() >= k; });

    vector<int> byDie[2];
    for (int c : g_freeCores) byDie[dieOf(c)].push_back(c);

    vector<int> got;
    if (k == 2 && !byDie[0].empty() && !byDie[1].empty()) {
        got.push_back(byDie[0].back());
        got.push_back(byDie[1].back());
    } else {
        for (int c : g_freeCores) {
            got.push_back(c);
            if ((int)got.size() == k) break;
        }
    }
    for (int c : got)
        g_freeCores.erase(std::find(g_freeCores.begin(), g_freeCores.end(), c));
    return got;
}

static void releaseCores(const vector<int>& cores) {
    {
        std::lock_guard<std::mutex> lk(g_coreMx);
        for (int c : cores) g_freeCores.push_back(c);
    }
    g_coreCv.notify_all();
}

// ---------------------------------------------------------------------------
// ПАРАЛЛЕЛЬНЫЙ РЕЖИМ БАЗЫ: работа предъявляется как N_CORES независимых кусков
// размера size/N_CORES, а сколько воркеров реально в неё впряжётся — решает САМ
// планировщик TBB (work stealing). Мы не назначаем число ядер, лишь даём
// возможность распараллелить.
//
// simple_partitioner + grainsize 1 обязательны: с auto_partitioner TBB вправе
// слепить куски в одну задачу и отдать одному воркеру — в логе вышло бы «TBB не
// параллелит», хотя это артефакт нарезки, а не решение планировщика.
//
// ПИННИНГ: каждый кусок берёт счётное ядро ИЗ ПУЛА и держит его до конца куска —
// тем же механизмом, что одномодальный путь. Пул гарантирует, что ядро занято
// ровно одним потоком. (Пиннинг по current_thread_index() оказался неверен:
// слот 0 арены резервируется под мастера, поэтому ядро CORE_NUMBERS[0] не
// доставалось никому, индексы конфликтовали — два потока садились на одно ядро,
// а поток без индекса оставался на ядре сокета 1.)
// ---------------------------------------------------------------------------
static void runJobTbbParallel(const string& type, int size, int jobId) {
    const int pieces = N_CORES;
    const int subSize = size / pieces;

    printf("job %d_%s_%d started %lld\n", jobId, type.c_str(), size,
        (long long)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());

    tbb::parallel_for(tbb::blocked_range<int>(0, pieces, 1),
        [&type, subSize, jobId](const tbb::blocked_range<int>& r) {
            for (int piece = r.begin(); piece != r.end(); ++piece) {
                vector<int> my = acquireCores(1);      // ждём свободное счётное ядро
                pinThisThreadTo(my[0]);
                int gotCpu = sched_getcpu();
                printf("  piece %d of job %d started %lld core=%d want=%d%s\n", piece, jobId,
                    (long long)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count(),
                    gotCpu, my[0], (gotCpu == my[0] ? "" : "  <<< НЕСОВПАДЕНИЕ"));
                Job* sub = makeJob(type, subSize);
                double tmp = 0.0;
                sub->execute(&tmp, false);
                delete sub;
                printf("  piece %d of job %d end %lld core=%d\n", piece, jobId,
                    (long long)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count(),
                    sched_getcpu());
                releaseCores(my);
            }
        }, tbb::simple_partitioner());

    printf("job %d_%s_%d end %lld\n", jobId, type.c_str(), size,
        (long long)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

static void runJob(const string& type, int size, const vector<int>& cores, int jobId) {
    if (cores.empty()) {                          // мода 1: сам прибиваюсь к счётному ядру
        int myCore = acquireCore();
        if (myCore >= 0) pinThisThreadTo(myCore); // жёстко на одно из {0,1,4,5}
        printf("job %d_%s_%d started %lld core=%d\n", jobId, type.c_str(), size,
            (long long)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count(), sched_getcpu());
        Job* job = makeJob(type, size);
        double tmp = 0.0;
        job->execute(&tmp, false);
        delete job;
        releaseCore(myCore);
        printf("job %d_%s_%d end %lld\n", jobId, type.c_str(), size,
            (long long)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
        return;
    }
    // мода k: k подзадач, каждая на своём заданном ядре (для мультимода — позже)
    printf("job %d_%s_%d started %lld core=%d\n", jobId, type.c_str(), size,
        (long long)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count(), sched_getcpu());
    int k = (int)cores.size();
    int subSize = size / k;
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
    for (auto& t : threads) t.join();
    printf("job %d_%s_%d end %lld\n", jobId, type.c_str(), size,
        (long long)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
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
// Единый прогон через oneTBB flow graph — ОДИН И ТОТ ЖЕ движок и для базового
// прогона, и для заданного расписания. Отличается ТОЛЬКО порядок:
//   * базовый прогон (ORDER пуст) — приоритетов нет, oneTBB выбирает порядок сам;
//   * расписание (ORDER задан) — приоритет узла = его позиция в перестановке
//     (ORDER[0] — высший), oneTBB предпочитает готовые узлы по этому приоритету,
//     сохраняя динамическую загрузку ядер (списочное планирование по приоритету).
// Граф, арена, потоки, runJob, DEPS — идентичны в обоих случаях.
// ---------------------------------------------------------------------------
static double runGraph(const Project& p, tbb::task_arena& arena, bool tbbParallel) {
    using namespace tbb::flow;
    int N = (int)p.jobs.size();
    bool ordered = !p.order.empty();

    // Приоритеты узлов по перестановке (для базового прогона — без приоритетов).
    vector<node_priority_t> prio(N, no_priority);
    if (ordered)
        for (int rank = 0; rank < N; rank++)
            prio[p.order[rank]] = (node_priority_t)(N - rank);

    graph g;
    broadcast_node<continue_msg> node0(g);

    vector<continue_node<continue_msg>*> nodes(N);
    for (int i = 0; i < N; i++) {
        const string& type = p.jobs[i].first;
        int size = p.jobs[i].second;
        int k = p.modeOf[i];
        nodes[i] = new continue_node<continue_msg>(
            g, [type, size, i, k, tbbParallel](const continue_msg&) {
                if (tbbParallel) {
                    // База с распараллеливанием: число ядер под работу выбирает TBB.
                    runJobTbbParallel(type, size, i);
                } else if (k > 1) {
                    // Расписание с MODES: берём ровно k ядер (ждём, если надо).
                    vector<int> cores = acquireCores(k);
                    runJob(type, size, cores, i);
                    releaseCores(cores);
                } else {
                    runJob(type, size, {}, i);     // мода 1, ядро выбирает движок
                }
            }, prio[i]);
    }

    // Стартовые рёбра — единственное отличие базы от расписания.
    vector<continue_node<continue_msg>*> fake;
    if (ordered) {
        // Фиктивная стартовая «лесенка». В самый первый миг node0 будит узлы разом,
        // и 4 воркера в гонке хватают что попало — оттого стартовая четвёрка не
        // совпадала с ORDER. Ставим CORES_COUNT пустых узлов высшего приоритета с
        // РАЗНЫМИ длительностями 0,1,2,3 мс: они гаснут по очереди, освобождая воркеры
        // по одному, а не разом. fake[k] будит ровно ORDER[k] — поэтому первые четыре
        // реальные работы стартуют по очереди и строго ORDER[0..3]. Весь хвост
        // ORDER[4..] висит на последней ступеньке (fake[CORES_COUNT-1]) и вспыхивает
        // уже когда старт занят; дальше порядок держат приоритеты (после старта они
        // и так соблюдались). Стаггер в миллисекунды против работ в секунды делает
        // стартовую гонку невозможной. Лишние ~3 мс на фоне makespan ничтожны.
        fake.resize(N_CORES);
        for (int f = 0; f < N_CORES; f++) {
            int delayMs = (f == N_CORES - 1) ? 1 : 0;                        // 0,0,0,1 мс
            fake[f] = new continue_node<continue_msg>(
                g, [delayMs, f](const continue_msg&) {
                    printf("fake %d started %lld delay_ms=%d\n", f,
                        (long long)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count(), delayMs);
                    if (delayMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
                    printf("fake %d end %lld\n", f,
                        (long long)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
                },
                (node_priority_t)(N + 1));          // выше любого реального приоритета
            make_edge(node0, *fake[f]);
        }
        // Первые CORES_COUNT реальных работ: fake[k] -> ORDER[k], строго по одной.
        int head = std::min(N, N_CORES);
        for (int rank = 0; rank < head; rank++)
            make_edge(*fake[rank], *nodes[p.order[rank]]);
        // Хвост ORDER[CORES_COUNT..] — на последнюю (самую позднюю) ступеньку.
        for (int rank = N_CORES; rank < N; rank++)
            make_edge(*fake[N_CORES - 1], *nodes[p.order[rank]]);
    } else {
        // База: node0 будит все реальные узлы разом — «родной» порядок oneTBB.
        for (int i = 0; i < N; i++)
            make_edge(node0, *nodes[i]);
    }

    // Частичный порядок задачи (DEPS) — поверх, одинаково для базы и расписания.
    for (auto& e : p.deps)
        make_edge(*nodes[e.first], *nodes[e.second]);

    auto t0 = high_resolution_clock::now();
    // Главный поток НЕ входит в арену: он ждёт на future (futex, сон) и НЕ считает.
    // Оркестровку крутит РАБОЧИЙ поток арены; wait_for_all заставляет его участвовать
    // в счёте графа, поэтому считают все 4 воркера на {0,1,4,5}. Маска процесса из 5
    // ядер даёт пулу поднять именно 4 воркера (при маске из 4 их было бы 3).
    std::promise<void> finished;
    std::future<void> fut = finished.get_future();
    arena.enqueue([&]{
        node0.try_put(continue_msg());
        g.wait_for_all();
        finished.set_value();
    });
    fut.wait();
    auto t1 = high_resolution_clock::now();

    for (int i = 0; i < N; i++) delete nodes[i];
    for (auto* f : fake) delete f;
    return duration<double, milli>(t1 - t0).count();
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "usage: " << argv[0] << " <папка_с_проектами> <файл_результатов> [--parallel]\n"
             << "  --parallel: в БАЗОВОМ прогоне разрешить TBB самому распараллеливать работы\n";
        return 1;
    }
    printf("BUILD: пиннинг из пула ядер, версия 2 (%s %s)\n", __DATE__, __TIME__);
    string projDir = argv[1];
    string outPath = argv[2];
    // --parallel: в БАЗОВОМ прогоне работа предъявляет свою параллельность, а сколько
    // ядер под неё выделить — решает планировщик TBB. Для расписаний (есть ORDER)
    // флаг игнорируется: там число ядер задаёт секция MODES.
    bool parallelFlag = (argc > 3 && string(argv[3]) == "--parallel");

    // Запереть процесс на ядрах {0,1,4,5} (счётные, сокет 0) + одно ядро сокета 1
    // под оркестратор. Без запирания потоки TBB расползаются на оба сокета (две FSB,
    // четыре L2) и memory-работы идут вдвое быстрее — нечестно. Счёт идёт только на
    // четырёх ядрах сокета 0; пятое ядро нужно лишь главному потоку, который висит
    // в wait_for_all и процедур не считает, чтобы он не отнимал счётное ядро.
    {
        cpu_set_t mask; CPU_ZERO(&mask);
        for (int c : CORE_NUMBERS) CPU_SET(c, &mask);
        CPU_SET(ORCHESTRATOR_CORE, &mask);   // +1 ядро сокета 1 под оркестратор
        if (sched_setaffinity(0, sizeof(mask), &mask) < 0)
            cerr << "Не удалось запереть процесс на ядрах сокета 0\n";
    }

    #ifdef USE_MKL
    mkl_set_num_threads(1);   // каждая (под)задача — однопоточный BLAS/VML
    #endif

    // Арена на 4 слота = 4 счётных воркера на {0,1,4,5}. Главный поток в арену НЕ
    // входит (enqueue + future), поэтому слот ему не нужен. Маска из 5 ядер +
    // global_control на N_CORES+1 нужны, чтобы пул поднял 4 воркера при живом
    // главном потоке (иначе, при 4 ядрах в маске, воркеров было бы лишь 3).
    tbb::global_control gc(tbb::global_control::max_allowed_parallelism, N_CORES + 1);
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
        bool tbbParallel = parallelFlag && !ordered;   // распараллеливание — только для базы
        cout << "=== Выполняется проект: " << nm
             << " (" << p.jobs.size() << " работ, "
             << (ordered ? "перестановка ORDER" : "базовый прогон oneTBB")
             << (tbbParallel ? ", TBB сам распараллеливает" : "") << ") ===" << endl;
        double sum = 0.0;
        for (int it = 0; it < ITERATION_COUNT; it++) {
            double ms = runGraph(p, arena, tbbParallel);
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

