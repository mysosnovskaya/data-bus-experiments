#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include "../common/ExecutionFlag.hpp"
#include "../common/Jobs.hpp"
#include "StatisticUtils.hpp"

using namespace std;
using namespace std::chrono;

template<typename T>
void printVector(vector<T>& v, ostream& out) {
    for (int i = 0; i < v.size(); ++i) {
        out << v[i];
        if (i < v.size() - 1) {
            out << ",";
        }
    }
}

int maxCoresCount = 4;

const int iterationCount = 30;

vector<Job*> jobs = {
    MklXpyJob::create(20), /*MklXpyJob::create(35), MklXpyJob::create(50),*/ MklXpyJob::create(65),
    MklSumJob::create(200), /*MklSumJob::create(350), MklSumJob::create(500),*/ MklSumJob::create(650),
    MklQrJob::create(1000), /*MklQrJob::create(1100), MklQrJob::create(1200),*/ MklQrJob::create(1300),
    MklCopyJob::create(10), /*MklCopyJob::create(25), MklCopyJob::create(40),*/ MklCopyJob::create(55)
};

vector<vector<int>> getAllPossibleModes(int maxValue, int maxSize) {
    vector<vector<int>> permutations = {};
    int size = 1;
    while (size <= maxSize) {
        vector<int> permutation = { 0 };
        while (!permutation.empty()) {
            if (permutation.back() == maxValue) {
                permutation.pop_back();
                if (!permutation.empty()) {
                    permutation.back() += 1;
                }
            }
            else if (permutation.size() < size) {
                permutation.push_back(permutation.back() + 1);
            }
            else {
                permutations.push_back(permutation);
                permutation.back() += 1;
            }
        }
        size += 1;
    }
    return permutations;
}

void executeJob(Job* job, pthread_barrier_t* barrier, int index, vector<double>* percentOfExecution) {
    pthread_barrier_wait(barrier);
    job->execute(&(*percentOfExecution)[index], true);
}

string getFileName(vector<int> jobIndexes) {
    auto jobsCountStr = to_string(jobIndexes.size());

    string jobsSizesString;
    for (int i = 0; i < jobIndexes.size(); i++) {
        jobsSizesString = jobsSizesString + "_" + jobs[jobIndexes[i]]->getJobId();
    }
    return string("results/data_") + string(jobsCountStr) + "j_" + jobsSizesString + string(".txt");
}

void printData(vector<vector<double>> durationsOfIterations, vector<int> jobIndexes) {
    string fileName = getFileName(jobIndexes);

    ofstream myfile;
    myfile.open(fileName);

    for (int i = 0; i < jobIndexes.size(); i++) {
        myfile << "Job " << jobs[jobIndexes[i]]->getJobId() << endl;
        printVector(durationsOfIterations[i], *&myfile);
        myfile << endl;

        double expectedValue = calculateExpectedValue(durationsOfIterations[i]);
        myfile << "Expected value: " << expectedValue << endl;
        myfile << "Dispersion: " << calculateDispersion(durationsOfIterations[i], expectedValue) << endl;
        myfile << "Standard Deviation: " << calculateStandardDeviation(durationsOfIterations[i], expectedValue) << endl;
        myfile << "Minimum value: " << calculateMinValue(durationsOfIterations[i]) << endl;
        myfile << "Maximum value: " << calculateMaxValue(durationsOfIterations[i]) << endl;
        myfile << endl;
    }

    myfile.close();
}

long run(vector<int> jobIndexes) {
    vector<string> jobTypes;
    for (int i = 0; i < jobIndexes.size(); i++) {
        jobTypes.push_back(jobs[jobIndexes[i]]->getJobId());
    }

    cout << "starting execution jobs ";
    printVector(jobTypes, *&cout);
    cout << endl;

    int average = 0;

    vector<vector<double>> jobsTime;
    for (int i = 0; i < jobIndexes.size(); i++) {
        jobsTime.push_back(vector<double>(iterationCount));
    }

    for (int i = 0; i < iterationCount; i++) {
        vector<double> percentOfExecution(jobIndexes.size());

        GLOBAL_EXECUTION_FLAG = false;
        vector<thread> threads;
        pthread_barrier_t barrier;
        pthread_barrier_init(&barrier, NULL, jobIndexes.size() + 1);
        for (int k = 0; k < jobIndexes.size(); k++) {
            Job* job = jobs[jobIndexes[k]];
            threads.push_back(thread(executeJob, job, &barrier, k, &percentOfExecution));
            // next 4 lines are Linux only, comment it for Windows
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(k, &cpuset);
            pthread_setaffinity_np(threads[k].native_handle(), sizeof(cpu_set_t), &cpuset);
        }

        high_resolution_clock::time_point startTime = high_resolution_clock::now();

        pthread_barrier_wait(barrier);
        for (int k = 0; k < jobIndexes.size(); k++) {
            threads[k].join();
        }

        high_resolution_clock::time_point endTime = high_resolution_clock::now();
        duration<double, std::milli> time = endTime - startTime;

        cout << "iteration " << i << " time: " << time.count() << endl;

        for (int j = 0; j < jobIndexes.size(); j++) {
            jobsTime[j][i] = ((double)time.count() / percentOfExecution[j]);
        }

        average += time.count();
        pthread_barrier_destroy(&barrier);
    }

    printData(jobsTime, jobIndexes);

    long result = (long)((double)average / iterationCount);

    cout << "average time for jobs [";
    printVector(jobTypes, *&cout);
    cout << "] is " << result << endl << endl;
    return result;
}

int main() {
    srand(unsigned(time(0)));

    vector<vector<int>> allPossibleModes = getAllPossibleModes(jobs.size(), maxCoresCount);

    cout << "all possible modes count: " << allPossibleModes.size() << endl << endl;

    for (int i = 0; i < allPossibleModes.size(); i++) {
        cout << "Starting mode: " << i + 1 << "/" << allPossibleModes.size() << endl;
        run(allPossibleModes[i]);
    }

    cout << "execution completed" << endl;

    return 0;
}
