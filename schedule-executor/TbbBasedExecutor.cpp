#include <dirent.h>
#include <string.h>
#include <string>
#include <vector>
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
#include <filesystem>
#include <sched.h>
#include <tbb/tbb.h>
#include <tbb/flow_graph.h>

using namespace std;
using namespace std::chrono;
namespace fs = std::filesystem;

std::vector<int> coreNumbersVector {
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39
};
tbb::concurrent_queue<int> coreNumbers(coreNumbersVector.begin(), coreNumbersVector.end());


class PinningObserver: public tbb::task_scheduler_observer {
public:
    PinningObserver() { observe(true); }

    void on_scheduler_entry(bool worker) {
        auto numberOfSlots = tbb::this_task_arena::max_concurrency();
        cpu_set_t *cpu_set = CPU_ALLOC(numberOfSlots);
        size_t setsize = CPU_ALLOC_SIZE(numberOfSlots);
        CPU_ZERO_S(setsize, cpu_set);
        int coreNumber = 0;
        coreNumbers.try_pop(coreNumber);
        CPU_SET_S(coreNumber, setsize, cpu_set);
        if (sched_setaffinity(0, setsize, cpu_set) < 0) {
            cerr << "Unable to Set Affinity" << endl;
        }
        CPU_FREE(cpu_set);
    }

    void on_scheduler_exit(bool worker) {
        auto numberOfSlots = tbb::this_task_arena::max_concurrency();
        cpu_set_t *cpu_set = CPU_ALLOC(numberOfSlots);
        size_t setsize = CPU_ALLOC_SIZE(numberOfSlots);
        CPU_ZERO_S(setsize, cpu_set);
        for (int coreNumber : coreNumbersVector) {
            CPU_SET_S(coreNumber, setsize, cpu_set);
        }
        if (sched_setaffinity(0, setsize, cpu_set) < 0) {
            cerr << "Unable to Set Affinity" << endl;
        }
        CPU_FREE(cpu_set);
    }
};

const int iterationCount = 30;

template<typename T>
void printVector(vector<T>& v, ostream& out) {
    for (int i = 0; i < v.size(); ++i) {
        out << v[i];
        if (i < v.size() - 1) {
            out << ",";
        }
    }
}

vector<long> run(vector<Job*> jobs) {
    long long averageTime = 0;

    vector<long> iterationDurations(iterationCount);

    for (int j = 0; j < iterationCount; j++) {
            cout << endl << endl << "start iteration " << j + 1 << endl << endl;

            tbb::flow::graph g;
            tbb::flow::continue_node< tbb::flow::continue_msg >
                node0(g, [](const tbb::flow::continue_msg&) { cout << "We are starting..." << endl; });

            vector<tbb::flow::continue_node<tbb::flow::continue_msg>*> nodes;
            for (int j = 0; j < jobs.size(); j++) {
                nodes.push_back(new tbb::flow::continue_node<tbb::flow::continue_msg>(g, [&jobs, j](const tbb::flow::continue_msg&) { jobs[j]->execute(j); }));
                tbb::flow::make_edge(node0, *(nodes[j]));
            }

            for (int j1 = 0; j1 < jobs.size(); j1++) {
                for (int j2 = 0; j2 < jobs.size(); j2++) {
                    if (order[j1][j2] == 1) {
                        tbb::flow::make_edge(*nodes[j2], *nodes[j1]);
                    }
                }
            }

            high_resolution_clock::time_point startTime = high_resolution_clock::now();

            node0.try_put(tbb::flow::continue_msg());
            g.wait_for_all();

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
    oneapi::tbb::global_control global_limit(oneapi::tbb::global_control::max_allowed_parallelism, coreNumbersVector.size());
    PinningObserver p;

    cerr << "Started" << endl;

    char path[] = "input_our_examples";
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

            readJobs(&inFile, jobsCount);
            readOrderTable(&inFile, jobsCount);
//            readDelays(&inFile, jobsCount);
//            readQueues(&inFile);

            inFile.close();

            cerr << file_name << " read. String execution..." << endl;
            vector<long> iterationDurations = run(jobs);
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
