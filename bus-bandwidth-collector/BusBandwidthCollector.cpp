#include <string>
#include <chrono>
#include <time.h>
#include <cstdlib>
#include <set>
#include <map>
#include <iostream>
#include <thread>
#include <fstream>
#include <cstring>
#include "../common/Jobs.hpp"
#include "../common/ExecutionFlag.hpp"
#include "../common/CorePresets.hpp"

using namespace std;
using namespace std::chrono;

vector<Job*> jobs = {
    MklXpyJob::create(20), MklXpyJob::create(35), MklXpyJob::create(50), MklXpyJob::create(65),
    MklSumJob::create(200), MklSumJob::create(350), MklSumJob::create(500), MklSumJob::create(650),
   // MklQrJob::create(1000), MklQrJob::create(1100), MklQrJob::create(1200), MklQrJob::create(1300),
    MklCopyJob::create(10), MklCopyJob::create(25), MklCopyJob::create(40), MklCopyJob::create(55),
    MklMulJob::create(1000), MklMulJob::create(1100), MklMulJob::create(1200), MklMulJob::create(1300)
};

const int ITERATION_COUNT_DEFAULT = 7;

void executeJob(Job* job, pthread_barrier_t* barrier) {
    pthread_barrier_wait(barrier);
    cout << "start Job " << job->getJobId() << endl;
    double tmp = 0;
    job->execute(&tmp, true);
}

long calculateStandardTime(Job* job) {
    cout << "calculate standard time" << endl;
    cout << "JobId is " << job->getJobId() << endl;
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
    return time.count();
}

long calculateBusPercent(Job* job, vector<int> coresNumbers) {
        vector<Job*> jobsToExecute;
        for (int core: coresNumbers) {
            jobsToExecute.push_back(job->copy());
        }
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

        for (int j = 0; j < jobsToExecute.size(); j++) {
            delete jobsToExecute[j];
        }

        return time.count();
}

int main(int argc, char** argv) {
    // required arguments
    string corePreset = "";

    // optional arguments
    int iterationCount = ITERATION_COUNT_DEFAULT;
    string outputFile = "bus-bandwidth-collector/results/bus_percents_results.txt";

    for (auto i = 0; i < argc; ++i) {
        auto name = argv[i];

        if (strcmp("--core-preset", name) == 0) {
            ++i;
            if (i < argc) {
                auto value = argv[i];
                corePreset = value;
            }
        }

        if (strcmp("--iteration-count", name) == 0) {
            ++i;
            if (i < argc) {
                auto value = argv[i];
                iterationCount = atoi(value);
            }
        }

        if (strcmp("--output-file", name) == 0) {
            ++i;
            if (i < argc) {
                auto value = argv[i];
                outputFile = value;
            }
        }
    }

    if (corePreset == "") {
        cerr << "Missing core-preset. See README.md for more information" << endl;
        return -1;
    }
    vector<int> coresNumbers = getCoreNumbers(corePreset);

    cout << "execution started" << endl;

    map<string, vector<long>> jobIdToIterationDurationsStandardTime;
    map<string, vector<long>> jobIdToIterationDurationsWithSlowdown;

    for (Job* job : jobs) {
        jobIdToIterationDurationsStandardTime[job->getJobId()] = {};
        jobIdToIterationDurationsWithSlowdown[job->getJobId()] = {};
    }

    for (int i = 0; i < iterationCount; i++) {
        for (Job* job : jobs) {
            cout << "start iteration " << i + 1 << "/" << iterationCount << " for job " << job->getJobId() << endl;
            long standardTime = calculateStandardTime(job);
            jobIdToIterationDurationsStandardTime[job->getJobId()].push_back(standardTime);
            long iterationDuration = calculateBusPercent(job, coresNumbers);
            jobIdToIterationDurationsWithSlowdown[job->getJobId()].push_back(iterationDuration);
        }
    }

    ofstream myfile;
    myfile.open(outputFile, ios_base::app);

    for (Job* job : jobs) {
        long iterationsTimeSum = 0;
        for (long iterationDuration : jobIdToIterationDurationsWithSlowdown[job->getJobId()]) {
            iterationsTimeSum += iterationDuration;
        }
        int averageTime = iterationsTimeSum / iterationCount;
        cout << "average execution time for job " << job->getJobId() << " is " << averageTime << endl;

        long standardTime = 0;
        for (long iterationDuration : jobIdToIterationDurationsStandardTime[job->getJobId()]) {
            standardTime += iterationDuration;
        }
        standardTime = standardTime / iterationCount;
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
        myfile << job->getJobId() << " " << standardTime << " " << busPercent << endl;
    }
    myfile.close();

    return 0;
}
