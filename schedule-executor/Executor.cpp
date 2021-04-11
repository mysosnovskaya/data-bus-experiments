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
#include <condition_variable>
#include <filesystem>

using namespace std;
using namespace std::chrono;
namespace fs = std::filesystem;

const int iterationCount = 50;
vector<double> delayis;
vector<Job*> jobs;
vector<vector<int>> order;
vector<thread> threads;
vector<bool> jobIsFinished;
vector<vector<int>> queue;

std::condition_variable cv;
std::mutex m;

template<typename T>
void printVector(vector<T>& v, ostream& out) {
    for (int i = 0; i < v.size(); ++i) {
        out << v[i];
        if (i < v.size() - 1) {
            out << ",";
        }
    }
}

void executeJob(vector<int> jobIndexes, pthread_barrier_t* barrier) {
    pthread_barrier_wait(barrier);
    for (int jobIndex : jobIndexes) {
        for (int j = 0; j < order.size(); j++) {
            if (order[j][jobIndex] == 1 && !jobIsFinished[j]) {
                std::unique_lock<std::mutex> lock(m);
                cv.wait(lock, [j] {return jobIsFinished[j]; });
            }
        }
        if (delayis[jobIndex] != 0) {
            this_thread::sleep_for(std::chrono::milliseconds((int) delayis[jobIndex]));
        }

        printf("thread: start job %d \n", jobIndex);
        jobs[jobIndex]->execute(jobIndex);
        jobIsFinished[jobIndex] = true;
        cv.notify_all();
    }
}

bool allJobsAreFinished() {
    for (int i = 0; i < jobs.size(); i++) {
        if (!jobIsFinished[i]) {
            return false;
        }
    }
    return true;
}

vector<long> run() {
    long long averageTime = 0;

    vector<long> iterationDurations(iterationCount);

    for (int j = 0; j < iterationCount; j++) {
        cout << endl << endl << "start iteration " << j + 1 << endl << endl;
        for (int i = 0; i < jobs.size(); i++) {
            jobIsFinished[i] = false;
        }

        pthread_barrier_t barrier;
        pthread_barrier_init(&barrier, NULL, jobIndexes.size() + 1);
        for (int k = 0; k < queue.size(); k++) {
            threads[k] = thread(executeJob, queue[k], &barrier);
            // next 4 lines are Linux only, comment it for Windows
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(k, &cpuset);
            pthread_setaffinity_np(threads[k].native_handle(), sizeof(cpu_set_t), &cpuset);
        }

        high_resolution_clock::time_point startTime = high_resolution_clock::now();

        pthread_barrier_wait(&barrier);

        for (int i = 0; i < threads.size(); i++) {
            threads[i].detach();
        }

        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [] {return allJobsAreFinished(); });

        high_resolution_clock::time_point endTime = high_resolution_clock::now();
        duration<double, std::milli> time = endTime - startTime;
        averageTime += time.count();
        iterationDurations[j] = time.count();

        cout << "iteration " << j + 1 << " finished for " << time.count() << " ms" << endl;
    }
    cout << "avarage time for all jobs is " << (averageTime / iterationCount) << " ms" << endl;
    return iterationDurations;
}

void readOrderTable(ifstream* inFile, int jobsCount) {
    order = vector<vector<int>>(jobsCount);

    for (int i = 0; i < jobsCount; i++) {
        order[i] = vector<int>(jobsCount);
    }

    for (int i = 0; i < jobsCount; i++) {
        for (int j = 0; j < jobsCount; j++) {
            *inFile >> order[i][j];
        }
    }
}

void readDelayis(ifstream* inFile, int jobsCount) {
    delayis = vector<double>(jobsCount);

    for (int i = 0; i < jobsCount; i++) {
        *inFile >> delayis[i];
    }
}

void readJobs(ifstream* inFile, int jobsCount) {
    jobs = vector<Job*>();
    jobIsFinished = vector<bool>(jobsCount);
    for (int i = 0; i < jobsCount; i++) {
        string jobType;
        int jobSize;
        *inFile >> jobSize;
        *inFile >> jobType;

        Job* job;
        if (jobType == "SUM") {
            job = MklSumJob::create(jobSize);
        }
        else if (jobType == "COPY") {
            job = MklCopyJob::create(jobSize);
        }
        else if (jobType == "XPY") {
            job = MklXpyJob::create(jobSize);
        }
        else {
            job = MklQrJob::create(jobSize);
        }
        jobs.push_back(job);
    }
}

void readQueues(ifstream* inFile) {
    int queueCount;
    *inFile >> queueCount;

    threads = vector<thread>(queueCount);
    vector<int> queueSize(queueCount);
    for (int i = 0; i < queueCount; i++) {
        *inFile >> queueSize[i];
    }

    queue = vector<vector<int>>(queueCount);
    for (int i = 0; i < queueCount; i++) {
        for (int j = 0; j < queueSize[i]; j++) {
            int x;
            *inFile >> x;
            queue[i].push_back(x);
        }
    }
}

int main() {
    srand(unsigned(time(0)));

    string path = "input";
    int count = 1;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        cout << "start to execute " << count << " : " << entry.path() << endl;
        
        ifstream inFile;
        inFile.open(entry.path());

        int jobsCount;
        inFile >> jobsCount;

        readOrderTable(&inFile, jobsCount);
        readDelayis(&inFile, jobsCount);
        readJobs(&inFile, jobsCount);
        readQueues(&inFile);

        inFile.close();

        vector<long> iterationDurations = run();

        ofstream myfile;
        myfile.open("results/executor_results.txt", ios_base::app);
        myfile << entry.path() << endl;
        printVector(iterationDurations, *&myfile);
        myfile.close();

        for (int i = 0; i < jobsCount; i++) {
            delete jobs[i];
        }
        count++;
    }

    return 0;
}