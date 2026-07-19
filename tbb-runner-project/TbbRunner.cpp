#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
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
static const int ITERATION_COUNT = 5;   // было 7

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
// Аффинити для потоков главной арены (мода-1 / свободные работы): каждый
// поток арены прибит к своему ядру по индексу (0..3 -> CORE_NUMBERS[idx]).
// Заменяет прежний вариант с очередью — устойчивее при пере-входах потоков.
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
//                   выбрал движок). Память создаётся лениво и сразу удаляется.
//   cores = k    -> мода k: k подзадач размера size/k, каждая на своём ядре
//                   (поток с аффинити), ждём завершения всех. Так же, как
//                   мерились замедления (пул-барьер по ядрам).
// ---------------------------------------------------------------------------
static void runJob(const string& type, int size, const vector<int>& cores) {
    if (cores.empty()) {                          // мода 1, ядро выбирает TBB
        Job* job = makeJob(type, size);
        double tmp = 0.0;
        job->execute(&tmp, false);
        delete job;
        return;
    }
    int k = (int)cores.size();                    // мода k
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
    for (auto& t : threads) t.join();
}

// ---------------------------------------------------------------------------
// Проект.
// ---------------------------------------------------------------------------
struct Project {
    string name;
    vector<pair<string,int>> jobs;   // (type, size) по id
    vector<pair<int,int>> deps;      // (src, dst)
    vector<vector<int>> coresOf;     // coresOf[id] = назначенные ядра (пусто = мода 1)
};

static Project parseProject(const string& path, const string& name) {
    Project p; p.name = name;
    ifstream in(path);
    string tag; int count;

    in >> tag >> count;              // JOBS N
    p.jobs.resize(count);
    p.coresOf.assign(count, {});
    for (int i = 0; i < count; i++) {
        int id; string type; int size;
        in >> id >> type >> size;
        p.jobs[id] = {type, size};
    }
    in >> tag >> count;              // DEPS M
    for (int i = 0; i < count; i++) {
        int s, d; in >> s >> d;
        p.deps.push_back({s, d});
    }
    in >> tag >> count;              // CORES K  (строка: id c1 [c2 ...])
    // Каждая запись CORES — это одна строка переменной длины, читаем построчно.
    string line; getline(in, line);  // дочитать конец строки "CORES K"
    for (int i = 0; i < count; i++) {
        getline(in, line);
        istringstream ss(line);
        int id; ss >> id;
        int c;
        while (ss >> c) p.coresOf[id].push_back(c);
    }
    return p;
}

// ---------------------------------------------------------------------------
// Один прогон проекта через oneTBB flow graph. Возвращает makespan (мс).
// Узел = работа; рёбра = зависимости; узлы без входящих зависимостей
// стартуют от node0. Тело узла вызывает runJob (мода 1 или k).
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
        const vector<int>& cores = p.coresOf[i];
        nodes[i] = new continue_node<continue_msg>(
            g, [type, size, &cores](const continue_msg&) {
                runJob(type, size, cores);
            });
        make_edge(node0, *nodes[i]);            // все связаны со стартом
    }
    for (auto& e : p.deps)                       // + рёбра зависимостей
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

    // все *.txt проекты из папки, по алфавиту
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
        cout << "=== Выполняется проект: " << nm
             << " (" << p.jobs.size() << " работ) ===" << endl;
        double sum = 0.0;
        for (int it = 0; it < ITERATION_COUNT; it++) {
            double ms = runOnce(p, arena);
            sum += ms;
            cout << "  итерация " << it << ": " << (long)ms << " ms" << endl;
        }
        long avg = (long)(sum / ITERATION_COUNT);
        cout << "  среднее по " << ITERATION_COUNT << " итерациям: " << avg << " ms" << endl;

        // В файл результатов — ТОЛЬКО имя и среднее.
        out << nm << " " << avg << "\n";
        out.flush();
    }
    cout << "execution completed -> " << outPath << endl;
    return 0;
}
