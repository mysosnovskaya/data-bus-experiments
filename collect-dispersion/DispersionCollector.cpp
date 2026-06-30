#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <numeric>
#include <pthread.h>
#include "../common/Jobs.hpp"

using namespace std;
using namespace std::chrono;

const int iterationCount = 7;

vector<int> coresNumbers = {
     0, 1, 2, 3
};

void executeJob(Job* job, pthread_barrier_t* barrier) {
pthread_barrier_wait(barrier);
    GLOBAL_EXECUTION_FLAG = false; 
    double tmp = 0;
    job->execute(&tmp, true);
}

long run(vector<Job*> jobs) {
     int average = 0;

    for (int i = 0; i < iterationCount; i++) {
        vector<thread> threads;
        pthread_barrier_t barrier;
        pthread_barrier_init(&barrier, NULL, jobs.size() + 1);

        for (size_t c = 0; c < jobs.size(); c++) {
            threads.push_back(thread(executeJob, jobs[c], &barrier));
            
            // Жесткая привязка каждого воркера к своему ядру сокета (0-3)
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(coresNumbers[c % coresNumbers.size()], &cpuset);
            pthread_setaffinity_np(threads.back().native_handle(), sizeof(cpu_set_t), &cpuset);
        }

        pthread_barrier_wait(&barrier);
        high_resolution_clock::time_point startTime = high_resolution_clock::now();

        for (thread& t: threads) {
            t.join();
        }

        high_resolution_clock::time_point endTime = high_resolution_clock::now();
        duration<double, std::milli> time = endTime - startTime;
        pthread_barrier_destroy(&barrier);

        cout << "iteration " << i << " time: " << time.count() << " ms" << endl;

        if (i > 1) {
            average += time.count();
        }
    }

    long result = (long)((double)average / (iterationCount - 2));

    cout << "average time is " << result << endl << endl;
    return result;
}

int main() {
    srand(unsigned(time(0)));

    // Значения по умолчанию
    string type = "";
    int size = -1;
    int cores = -1;

    // Парсинг флагов командной строки
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--type" && i + 1 < argc) {
            type = argv[++i];
        } else if (arg == "--size" && i + 1 < argc) {
            size = stoi(argv[++i]);
        } else if (arg == "--cores" && i + 1 < argc) {
            cores = stoi(argv[++i]);
        }
    }

    #ifdef USE_MKL
    mkl_set_num_threads(1);
    #endif

    // Вычисляем корректный размер подзадачи в зависимости от типа
    int targetSize = size;
    if (type == "GEMV" || type == "QR") {
        // Квадратичное деление площади матрицы на число ядер
        targetSize = static_cast<int>(size / std::sqrt(cores));
    } else {
        // Линейное деление векторов
        targetSize = size / cores;
    }

    vector<Job*> jobs;
    for (int i = 0; i < cores; i++) {
        Job* job = nullptr;
        if (type == "COPY")      job = MklCopyJob::create(targetSize);
        else if (type == "QR")   job = MklQrJob::create(targetSize);
        else if (type == "SUM")  job = MklSumJob::create(targetSize);
        else if (type == "DOT")  job = MklDotJob::create(targetSize);
        else if (type == "GEMV") job = MklGemvJob::create(targetSize);
        else                     job = MklXpyJob::create(targetSize);
        jobs.push_back(job);
    }

    cout << "Starting execution: " << cores << "x [" << jobs[0]->getJobId() << "]" << endl;

    // Запуск эксперимента
    run(jobs, to_string(size) + "_" + type + "_cores_" + to_string(cores));

    // Освобождение памяти
    for (Job* job : jobs) {
        delete job;
    }

    cout << "execution completed" << endl;

    return 0;
}
