#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <thread>
#include <pthread.h>
#include "../common/Jobs.hpp"

using namespace std;
using namespace std::chrono;

const int iterationCount = 5;
vector<int> coresNumbers = {0, 1, 4, 5};

// Описание одной работы в смеси
struct WorkSpec {
    string type;
    int size;
    int cores;
};

// Фабрика подзадачи (та же логика деления, что была в старом main)
Job* makeJob(const string& type, int targetSize) {
    if (type == "COPY")        return MklCopyJob::create(targetSize);
    else if (type == "SUM")    return MklSumJob::create(targetSize);
    else if (type == "DOT")    return MklDotJob::create(targetSize);
    else if (type == "XPY")    return MklXpyJob::create(targetSize);
    else if (type == "SQRTX")  return MklSqrtJob::create(targetSize);
    else if (type == "ERF")    return MklErfJob::create(targetSize);
    else if (type == "TGAMMA") return MklTgammaJob::create(targetSize);
    else if (type == "POW")    return MklPowJob::create(targetSize);
    else                       return MklXpyJob::create(targetSize);
}

int subSize(const string& type, int size, int cores) {
    if (type == "GEMV" || type == "QR")
        return static_cast<int>(size / std::sqrt(cores));
    return size / cores;
}

// Каждый воркер пишет своё время в per-worker слот -> усредняем по работе
void executeJob(Job* job, pthread_barrier_t* barrier, double* outMs) {
    pthread_barrier_wait(barrier);
    high_resolution_clock::time_point t0 = high_resolution_clock::now();
    double tmp = 0;
    job->execute(&tmp, true);
    high_resolution_clock::time_point t1 = high_resolution_clock::now();
    *outMs = duration<double, std::milli>(t1 - t0).count();
}

int main(int argc, char* argv[]) {
    // --- Разбор аргумента --jobs "TYPE:SIZE:CORES,TYPE:SIZE:CORES,..." ---
    string jobsArg;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--jobs" && i + 1 < argc) jobsArg = argv[++i];
    }
    if (jobsArg.empty()) {
        cerr << "usage: --jobs TYPE:SIZE:CORES,TYPE:SIZE:CORES,..." << endl;
        return 1;
    }

    vector<WorkSpec> specs;
    stringstream ss(jobsArg);
    string token;
    while (getline(ss, token, ',')) {
        // token = TYPE:SIZE:CORES
        size_t p1 = token.find(':'), p2 = token.rfind(':');
        WorkSpec w;
        w.type  = token.substr(0, p1);
        w.size  = stoi(token.substr(p1 + 1, p2 - p1 - 1));
        w.cores = stoi(token.substr(p2 + 1));
        specs.push_back(w);
    }

    // --- Проверка: суммарно не больше 4 ядер ---
    int totalCores = 0;
    for (auto& w : specs) totalCores += w.cores;
    if (totalCores > 4) {
        cerr << "ERROR: total cores " << totalCores << " > 4" << endl;
        return 2;
    }

    #ifdef USE_MKL
    mkl_set_num_threads(1);
    #endif

    // --- Создаём все воркеры, помним, какой воркер какой работе принадлежит ---
    vector<Job*> jobs;          // все потоки-воркеры подряд
    vector<int>  jobOwner;      // индекс работы для каждого воркера
    vector<string> jobLabel;    // подпись работы (TYPE_SIZE_CORES)
    for (size_t s = 0; s < specs.size(); s++) {
        int ts = subSize(specs[s].type, specs[s].size, specs[s].cores);
        for (int c = 0; c < specs[s].cores; c++) {
            jobs.push_back(makeJob(specs[s].type, ts));
            jobOwner.push_back((int)s);
        }
        jobLabel.push_back(specs[s].type + "_" + to_string(specs[s].size)
                           + "_c" + to_string(specs[s].cores));
    }
    int W = (int)jobs.size();   // всего воркеров (= totalCores)

    // Заголовок конфигурации
    cout << "CONFIG";
    for (size_t s = 0; s < specs.size(); s++)
        cout << " " << jobLabel[s];
    cout << endl;

    // --- Усреднение по работам за iterationCount прогонов ---
    vector<double> sumPerSpec(specs.size(), 0.0);

    for (int it = 0; it < iterationCount; it++) {
        vector<thread> threads;
        vector<double> workerMs(W, 0.0);
        pthread_barrier_t barrier;
        pthread_barrier_init(&barrier, NULL, W + 1);

        int coreCursor = 0;   // глобальный счётчик ядер: работы НЕ пересекаются
        for (int w = 0; w < W; w++) {
            threads.push_back(thread(executeJob, jobs[w], &barrier, &workerMs[w]));
            cpu_set_t cs; CPU_ZERO(&cs);
            CPU_SET(coresNumbers[coreCursor % coresNumbers.size()], &cs);
            pthread_setaffinity_np(threads.back().native_handle(),
                                   sizeof(cpu_set_t), &cs);
            coreCursor++;
        }

        pthread_barrier_wait(&barrier);
        for (auto& t : threads) t.join();
        pthread_barrier_destroy(&barrier);

        // Время работы = максимум по её воркерам (когда закончился последний)
        vector<double> specMs(specs.size(), 0.0);
        for (int w = 0; w < W; w++)
            specMs[jobOwner[w]] = max(specMs[jobOwner[w]], workerMs[w]);
        for (size_t s = 0; s < specs.size(); s++)
            sumPerSpec[s] += specMs[s];
    }

    // --- Вывод: по строке на работу ---
    for (size_t s = 0; s < specs.size(); s++) {
        double avg = sumPerSpec[s] / iterationCount;
        cout << "RESULT " << jobLabel[s] << " " << (long)avg << endl;
    }

    for (Job* job : jobs) delete job;
    cout << "END" << endl << endl;
    return 0;
}
