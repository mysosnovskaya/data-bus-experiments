#include <string>
#include <chrono>
#include <time.h>
#include <cstdlib>
#include <set>
#include <map>
#include <algorithm>
#include <random>
#include <iostream>
#include <thread>
#include <fstream>
#include <iomanip>
#include "../common/Jobs.hpp"
#include "../common/ExecutionFlag.hpp"
#include <condition_variable>

using namespace std;
using namespace std::chrono;

vector<Job*> jobs = {
    MklXpyJob::create(20), MklXpyJob::create(35), MklXpyJob::create(50), MklXpyJob::create(65),
    MklSumJob::create(200), MklSumJob::create(350), MklSumJob::create(500), MklSumJob::create(650),
    MklQrJob::create(1000), MklQrJob::create(1100), MklQrJob::create(1200), MklQrJob::create(1300),
    MklCopyJob::create(10), MklCopyJob::create(25), MklCopyJob::create(40), MklCopyJob::create(55)
};

vector<int> coresNumbers = {
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
    50, 51, 52, 53, 54, 55, 56, 57, 58, 59
};
const int iterationCount = 7;

void executeJob(Job* job, pthread_barrier_t* barrier) {
    pthread_barrier_wait(barrier);
    job->execute(0);
}

long calculateStandardTime(Job* job) {
    cout << "calculate standard time" << endl;
    map<string, long> jobSizeToStandardTime;
    cout << "JobId is " << job->getJobId() << endl;

    int average = 0;
    for (int i = 0; i < iterationCount; i++) {
        GLOBAL_EXECUTION_FLAG = false;
        pthread_barrier_t barrier;
        pthread_barrier_init(&barrier, NULL, 2);
        thread t(executeJob, job, &barrier);
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(0, &cpuset);
        pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpuset);
        high_resolution_clock::time_point startTime = high_resolution_clock::now();
        pthread_barrier_wait(&barrier);
        t.join();
        high_resolution_clock::time_point endTime = high_resolution_clock::now();
        duration<double, std::milli> time = endTime - startTime;
        cout << "iteration " << i << " time: " << time.count() << endl;
        average += time.count();
    }
    long result = (long)((double)average / iterationCount);
    cout << "standard time for job " << job->getJobId() << " is " << result << endl;

    return result;
}

int main() {
    cout << "execution started" << endl;

    for (Job* job : jobs) {
        long standardTime = calculateStandardTime(job);
        vector<Job*> jobsToExecute;
        for (int core: coresNumbers) {
            jobsToExecute.push_back(job->copy());
        }
        int iterationsTimeSum = 0;
        for (int i = 0; i < iterationCount; i++) {
            vector<thread> threads;

            pthread_barrier_t barrier;
            pthread_barrier_init(&barrier, NULL, coresNumbers.size() + 1);

            for (int c = 0; c < coresNumbers.size(); c++) {
                threads.push_back(thread(executeJob, jobsToExecute[c], &barrier));
                // next 4 lines are Linux only, comment it for Windows
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                CPU_SET(coresNumbers[c], &cpuset);
                pthread_setaffinity_np(threads.back().native_handle(), sizeof(cpu_set_t), &cpuset);
            }

            high_resolution_clock::time_point startTime = high_resolution_clock::now();

            pthread_barrier_wait(&barrier);
            for (thread& t: threads) {
                t.join();
            }

            high_resolution_clock::time_point endTime = high_resolution_clock::now();
            duration<double, std::milli> time = endTime - startTime;
            iterationsTimeSum += time.count();
        }

        for (int j = 0; j < jobsToExecute.size(); j++) {
            delete jobsToExecute[j];
        }

        int averageTime = iterationsTimeSum / iterationCount;
        cout << "average execution time for job " << job->getJobId() << " is " << averageTime << endl;

        double slowdown = (double) standardTime / averageTime;

        cout << "slowdown for job " << job->getJobId() << " is " << slowdown << endl << endl;

        double busPercent;
        if (slowdown < (double) 1 / coresNumbers.size()) {
            busPercent = 0;
        }
        else {
            busPercent = 100 / (slowdown * coresNumbers.size());
        }

        cout << "bus consumption for job " << job->getJobId() << " is " << busPercent << endl;

        ofstream myfile;
        myfile.open("results/bus_percents_results.txt", ios_base::app);
        myfile << job->getJobId() << " " << standardTime << " " << busPercent << endl;
        myfile.close();
    }
    return 0;
}
