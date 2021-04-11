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
#include "InputReader.hpp"
#include <condition_variable>
#include <filesystem>

using namespace std;
using namespace std::chrono;
namespace fs = std::filesystem;

const int iterationCount = 40;

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
        pthread_barrier_init(&barrier, NULL, queue.size() + 1);
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

        cout << endl << "iteration " << j + 1 << " finished for " << time.count() << " ms" << endl;
    }
    cout << "avarage time for all jobs is " << (averageTime / iterationCount) << " ms" << endl << endl;
    return iterationDurations;
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
        myfile << endl << endl;
        myfile.close();

        for (int i = 0; i < jobsCount; i++) {
            delete jobs[i];
        }
        count++;
    }

    return 0;
}
