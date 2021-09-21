#include <dirent.h>
#include <string.h>
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

vector<int> coresNumbers = {
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
    50, 51, 52, 53, 54, 55, 56, 57, 58, 59
};

const int iterationCount = 30;

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
    cerr << "start executeJob ";
    printVector(jobIndexes, *&cerr);
    cerr << endl;
    for (int jobIndex : jobIndexes) {
        for (int j = 0; j < order.size(); j++) {
            if (order[j][jobIndex] == 1 && !jobIsFinished[j]) {
                cerr << "waiting to execute job " << jobIndex << endl;
                std::unique_lock<std::mutex> lock(m);
                cv.wait(lock, [j] {return jobIsFinished[j]; });
            }
        }
        if (delays[jobIndex] != 0) {
            cerr << "delaying for job " << jobIndex << " is " << delays[jobIndex] << endl;
            this_thread::sleep_for(std::chrono::milliseconds((int) delays[jobIndex]));
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
        cerr << endl << endl << "start iteration " << j + 1 << endl << endl;
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
            CPU_SET(coresNumbers[k], &cpuset);
            pthread_setaffinity_np(threads[k].native_handle(), sizeof(cpu_set_t), &cpuset);
            cerr << "therad " << k << " created on core " << coresNumbers[k] << endl;
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
        cerr << endl << "iteration " << j + 1 << " finished for " << time.count() << " ms" << endl;
    }
    cout << "avarage time for all jobs is " << (averageTime / iterationCount) << " ms" << endl << endl;
    return iterationDurations;
}

int main() {
    srand(unsigned(time(0)));

    cerr << "Started" << endl;

    char path[] = "input_from_ga";
    int count = 1;

    struct dirent *dir;
    DIR *d = opendir(path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            string file_name(dir->d_name);
            if (file_name.find(".txt") == std::string::npos) {
                continue;
            }
            string file_path = string(path) + "/" + file_name;

            cout << "start to execute " << count << " : " << file_name << endl;
            cerr << "start to execute " << count << " : " << file_name << endl;

            ifstream inFile;
            inFile.open(file_path);

            int jobsCount;
            inFile >> jobsCount;

            readOrderTable(&inFile, jobsCount);
            readDelays(&inFile, jobsCount);
            readJobs(&inFile, jobsCount);
            readQueues(&inFile);

            inFile.close();

            cerr << file_name << " read. String execution..." << endl;
            vector<long> iterationDurations = run();
            cerr << file_name << " executed. String writing to the file..." << endl;

            ofstream myfile;
            myfile.open("results/ga_schedules_executor_results.txt", ios_base::app);
            myfile << file_name << endl;
            printVector(iterationDurations, *&myfile);
            myfile << endl << endl;
            myfile.close();

            cerr << file_name << " wrote. String deleting..." << endl;
            for (int i = 0; i < jobsCount; i++) {
                delete jobs[i];
            }
            count++;
            cerr << file_name << " execution completed." << endl;
        }
        closedir(d);
    } else {
        cerr << "Failed to open dir: " << path << endl;
    }

    cerr << "Completed" << endl;

    return 0;
}
