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
    MklQrJob::create(1000), MklQrJob::create(1100), MklQrJob::create(1200), MklQrJob::create(1300),
    MklCopyJob::create(10), MklCopyJob::create(25), MklCopyJob::create(40), MklCopyJob::create(55),
    MklMulJob::create(1000), MklMulJob::create(1100), MklMulJob::create(1200), MklMulJob::create(1300)
};

const int ITERATION_COUNT_DEFAULT = 7;

void executeJob(Job* job, pthread_barrier_t* barrier) {
    pthread_barrier_wait(barrier);
    job->execute(0);
}

long calculateStandardTime(Job* job, int iterationCount) {
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

void calculateBusPercent(Job* job, vector<int> coresNumbers, int iterationCount, string outputFile) {
        long standardTime = calculateStandardTime(job, iterationCount);
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
        myfile.open(outputFile, ios_base::app);
        myfile << job->getJobId() << " " << standardTime << " " << busPercent << endl;
        myfile.close();
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

    for (Job* job : jobs) {
        calculateBusPercent(job, coresNumbers, iterationCount, outputFile);
    }
    return 0;
}
