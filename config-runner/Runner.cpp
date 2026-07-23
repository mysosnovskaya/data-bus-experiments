#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <atomic>
#include <numeric>
#include <random>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <thread>
#include <pthread.h>
#include "../common/Jobs.hpp"

using namespace std;
using namespace std::chrono;

const int iterationCount = 5;

// --- Физические ядра, сгруппированные по die (общий L2 внутри пары) ---
// dieA = {0,4}, dieB = {1,5} — установлено по /proc/cpuinfo и подтверждено
// поведением реальных замеров (асимметрия mode=3 в самых первых прогонах).
vector<int> dieA = {0, 4};
vector<int> dieB = {1, 5};

// SOLO-режим (--jobs с ОДНОЙ работой) сохраняет РОВНО тот порядок, которым
// собран существующий бейзлайн ("Общая сводка") — {0,1,4,5}. На практике
// соло-прогоны в run.sh больше не генерируются (бейзлайн берём из старых
// данных), но путь оставлен рабочим на случай ручной проверки/сверки.
vector<int> coresSolo = {0, 1, 4, 5};

struct WorkSpec {
    string type;
    int size;
    int cores;
};

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

// ============================================================================
// Назначение ядер для MIX-режима (несколько работ одновременно).
//
// Единственное жёсткое правило: работа, которой досталось РОВНО 2 ядра,
// обязательно получает по одному ядру с КАЖДОЙ die-пары (разный L2) —
// иначе её собственная внутренняя картина исказится общим кэшем внутри
// самой себя. Всё остальное — перемешано (порядок работ, порядок ядер
// внутри пары), чтобы не вносить систематическое смещение в пользу
// конкретной физической позиции. Шафл делается один раз на конфигурацию,
// см. main().
// ============================================================================
vector<vector<int>> assignMixCores(const vector<WorkSpec>& specs, mt19937& rng) {
    vector<int> poolA = dieA;
    vector<int> poolB = dieB;
    std::shuffle(poolA.begin(), poolA.end(), rng);
    std::shuffle(poolB.begin(), poolB.end(), rng);

    vector<int> order(specs.size());
    iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);

    vector<vector<int>> result(specs.size());
    size_t ai = 0, bi = 0;

    // Шаг 1: РОВНО 2-ядерные работы — гарантированно оба ядра с die-пары.
    for (int idx : order) {
        if (specs[idx].cores == 2 && ai == 0) {
            result[idx] = { poolA[ai++], poolA[ai++] };
        }
    }

    // Шаг 2: всё оставшееся — общий пул, перемешанный ещё раз.
    vector<int> rest;
    while (ai < poolA.size()) rest.push_back(poolA[ai++]);
    while (bi < poolB.size()) rest.push_back(poolB[bi++]);
    std::shuffle(rest.begin(), rest.end(), rng);

    size_t ri = 0;
    for (int idx : order) {
        if (!result[idx].empty()) continue;
        for (int c = 0; c < specs[idx].cores; c++) {
            result[idx].push_back(rest[ri++]);
        }
    }
    return result;
}

// ============================================================================
// РАУНД: механизм "перезапуск до первого финиша самой медленной работы".
//
// Job::execute() всегда делает РОВНО 100 итераций (Jobs.hpp не менялся).
// Проблема, которую это решает: при однократном запуске всех работ по
// одному разу самая короткая работа заканчивается первой и дальше уже не
// создаёт конкуренции за шину — а значит время самой ДЛИННОЙ работы
// частично измеряется БЕЗ конкуренции, занижая её реальное замедление.
//
// Решение: каждый воркер, закончив свои 100 итераций, НЕМЕДЛЕННО
// перезапускается (снова 100 итераций), поддерживая полную конкуренцию за
// шину без пауз. Логируется (учитывается в качестве измерения) только
// ПЕРВОЕ завершение каждой работы — это её "чистое" время под честной
// конкуренцией: пока идёт это самое первое завершение, ВСЕ остальные
// работы (в том числе более быстрые, которые уже успели перезапуститься
// один или несколько раз) гарантированно ещё активны, потому что самая
// медленная работа по определению не может закончить свой первый прогон
// раньше любой другой.
//
// Как только у ВСЕХ работ учтено первое завершение (что происходит ровно
// в момент завершения самой медленной), поднимается общий флаг остановки.
// Воркеры, уже начавшие СЛЕДУЮЩИЙ (незалогированный) прогон к этому
// моменту, доигрывают его до конца естественным образом (не прерываются
// на полпути) и только потом останавливаются, не запускаясь снова.
//
// Для многоядерной работы "первое завершение" — момент, когда ПОСЛЕДНИЙ
// (самый медленный) из её СОБСТВЕННЫХ воркеров закончил первый прогон;
// логируется максимум по времени первого прогона среди своих воркеров —
// прямой аналог того, как замерялся солo-бейзлайн.
// ============================================================================
struct SpecRoundState {
    int workersDoneFirst = 0;
    double maxFirstMs = 0.0;
    bool loggedFirst = false;
};

void workerLoop(Job* job, int specIdx, const vector<WorkSpec>* specs,
                 pthread_barrier_t* startBarrier,
                 mutex* stateMutex, vector<SpecRoundState>* state,
                 atomic<int>* specsCompletedFirst, atomic<bool>* stopFlag) {
    pthread_barrier_wait(startBarrier);

    bool isFirstForThisWorker = true;
    while (true) {
        double tmp = 0;
        high_resolution_clock::time_point t0 = high_resolution_clock::now();
        job->execute(&tmp, true);
        high_resolution_clock::time_point t1 = high_resolution_clock::now();
        double elapsedMs = duration<double, std::milli>(t1 - t0).count();

        if (isFirstForThisWorker) {
            isFirstForThisWorker = false;
            bool specJustCompleted = false;
            {
                lock_guard<mutex> lk(*stateMutex);
                auto& st = (*state)[specIdx];
                st.workersDoneFirst++;
                st.maxFirstMs = max(st.maxFirstMs, elapsedMs);
                if (st.workersDoneFirst == (*specs)[specIdx].cores && !st.loggedFirst) {
                    st.loggedFirst = true;
                    specJustCompleted = true;
                }
            }
            if (specJustCompleted) {
                int done = ++(*specsCompletedFirst);
                if (done == (int)specs->size()) {
                    stopFlag->store(true);
                }
            }
        }

        if (stopFlag->load()) break; // не перезапускаемся, если сигнал уже дан
        // иначе — сразу заново, без ожидания соседей по своей же работе
    }
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
        size_t p1 = token.find(':'), p2 = token.rfind(':');
        WorkSpec w;
        w.type  = token.substr(0, p1);
        w.size  = stoi(token.substr(p1 + 1, p2 - p1 - 1));
        w.cores = stoi(token.substr(p2 + 1));
        specs.push_back(w);
    }

    int totalCores = 0;
    for (auto& w : specs) totalCores += w.cores;
    if (totalCores > 4) {
        cerr << "ERROR: total cores " << totalCores << " > 4" << endl;
        return 2;
    }
    if (specs.empty()) {
        cerr << "ERROR: no jobs parsed from --jobs" << endl;
        return 3;
    }

    bool isMix = (specs.size() > 1);

    #ifdef USE_MKL
    mkl_set_num_threads(1);
    #endif

    // --- Назначение ядер: один раз на конфигурацию ---
    vector<vector<int>> jobCores(specs.size());
    if (isMix) {
        std::random_device rd;
        std::mt19937 rng(rd());
        jobCores = assignMixCores(specs, rng);
    } else {
        int cursor = 0;
        for (size_t s = 0; s < specs.size(); s++) {
            for (int c = 0; c < specs[s].cores; c++) {
                jobCores[s].push_back(coresSolo[cursor % coresSolo.size()]);
                cursor++;
            }
        }
    }

    // --- Создаём воркеров ---
    vector<Job*> jobs;
    vector<int>  jobOwner;
    vector<int>  workerCoreId;
    vector<string> jobLabel;

    for (size_t s = 0; s < specs.size(); s++) {
        int ts = subSize(specs[s].type, specs[s].size, specs[s].cores);
        for (int c = 0; c < specs[s].cores; c++) {
            jobs.push_back(makeJob(specs[s].type, ts));
            jobOwner.push_back((int)s);
            workerCoreId.push_back(jobCores[s][c]);
        }
        jobLabel.push_back(specs[s].type + "_" + to_string(specs[s].size)
                           + "_c" + to_string(specs[s].cores));
    }
    int W = (int)jobs.size();

    cout << "CONFIG " << (isMix ? "MIX" : "SOLO");
    for (size_t s = 0; s < specs.size(); s++)
        cout << " " << jobLabel[s];
    cout << endl;

    cout << "CORES";
    for (size_t s = 0; s < specs.size(); s++) {
        cout << " " << jobLabel[s] << "=";
        for (size_t k = 0; k < jobCores[s].size(); k++) {
            if (k) cout << ",";
            cout << jobCores[s][k];
        }
    }
    cout << endl;

    vector<double> sumMsPerSpec(specs.size(), 0.0);

    for (int it = 0; it < iterationCount; it++) {
        mutex stateMutex;
        vector<SpecRoundState> state(specs.size());
        atomic<int> specsCompletedFirst(0);
        atomic<bool> stopFlag(false);

        pthread_barrier_t barrier;
        pthread_barrier_init(&barrier, NULL, W + 1);

        vector<thread> threads;
        for (int w = 0; w < W; w++) {
            threads.push_back(thread(workerLoop, jobs[w], jobOwner[w], &specs,
                                      &barrier, &stateMutex, &state,
                                      &specsCompletedFirst, &stopFlag));
            cpu_set_t cs; CPU_ZERO(&cs);
            CPU_SET(workerCoreId[w], &cs);
            pthread_setaffinity_np(threads.back().native_handle(),
                                   sizeof(cpu_set_t), &cs);
        }

        pthread_barrier_wait(&barrier);
        for (auto& t : threads) t.join();
        pthread_barrier_destroy(&barrier);

        for (size_t s = 0; s < specs.size(); s++)
            sumMsPerSpec[s] += state[s].maxFirstMs;
    }

    // --- Вывод: по строке на работу — среднее время первого честного
    // завершения под полной конкуренцией (мс). Формат идентичен старому
    // "Общей сводке" — прямое сравнение без пересчёта.
    for (size_t s = 0; s < specs.size(); s++) {
        double avgMs = sumMsPerSpec[s] / iterationCount;
        char buf[256];
        snprintf(buf, sizeof(buf), "RESULT %s %ld", jobLabel[s].c_str(), (long)avgMs);
        cout << buf << endl;
    }

    for (Job* job : jobs) delete job;
    cout << "END" << endl << endl;
    return 0;
}