#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include "OrderUtils.hpp"
#include "JobsGenerator.hpp"
#include "../common/Jobs.hpp"
#include <sched.h>
#include <tbb/tbb.h>
#include <tbb/flow_graph.h>

using namespace std;
using namespace std::chrono;

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
        CPU_ZERO(cpu_set);
        int coreNumber = 0;
        coreNumbers.try_pop(coreNumber);
        CPU_SET(coreNumber, cpu_set);
        if (sched_setaffinity(0, sizeof(cpu_set_t), cpu_set) < 0) {
            cerr << "Unable to Set Affinity" << endl;
        }
        CPU_FREE(cpu_set);
    }

    void on_scheduler_exit(bool worker) {
        auto numberOfSlots = tbb::this_task_arena::max_concurrency();
        cpu_set_t *cpu_set = CPU_ALLOC(numberOfSlots);
        CPU_ZERO(cpu_set);
        for (int coreNumber : coreNumbersVector) {
            CPU_SET(coreNumber, cpu_set);
        }
        if (sched_setaffinity(0, sizeof(cpu_set_t), cpu_set) < 0) {
            cerr << "Unable to Set Affinity" << endl;
        }
        CPU_FREE(cpu_set);
    }
};

template<typename T>
void printVector(vector<T>& v, ostream& out) {
    for (int i = 0; i < v.size(); ++i) {
        out << v[i];
        if (i < v.size() - 1) {
            out << ",";
        }
    }
}

vector<string> partialOrderTypes = { "NO_ORDER", "RANDOM", "BITREE", "ONE_TO_MANY_TO_ONE" };
vector<int> jobsCountArray = { 50, 100, 200 };
int optionsCount = 10;
const int iterationCount = 7;

string getFileName(string partialOrderType, int jobsCount, int optionNumber) {
    auto jobsCountStr = to_string(jobsCount);
    auto optionNumberStr = to_string(optionNumber);

    return string("results/tbb_output_") + string(jobsCountStr) + "j_" + partialOrderType + "_" + optionNumberStr + string(".txt");
}

void printData(vector<double> durations, vector<vector<int>> orderTable, string partialOrderType, vector<Job*> jobs, int optionNumber) {
    string fileName = getFileName(partialOrderType, jobs.size(), optionNumber);

    ofstream myfile;
    myfile.open(fileName);

    myfile << "Jobs: ";
    for (int i = 0; i < jobs.size(); i++) {
        myfile << jobs[i]->getJobId() << "  ";
    }
    myfile << endl;
    myfile << endl;
    printVector(durations, *&myfile);
    myfile << endl;
    myfile << endl;
    printOrderTable(orderTable, *&myfile);
    myfile.close();
}

vector<double> run(vector<Job*> jobs, vector<vector<int>> orderTable) {
    vector<string> jobIds;
    for (int i = 0; i < jobs.size(); i++) {
        jobIds.push_back(jobs[i]->getJobId());
    }
    cout << "Execution of jobs ";
    printVector(jobIds, *&cout);
    cout << endl;

    int averageTime = 0;
    vector<double> durations;

    for (int i = 0; i < iterationCount; i++) {
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
                if (orderTable[j1][j2] == 1) {
                    tbb::flow::make_edge(*nodes[j2], *nodes[j1]);
                }
            }
        }

        high_resolution_clock::time_point startTime = high_resolution_clock::now();

        node0.try_put(tbb::flow::continue_msg());
        g.wait_for_all();

        high_resolution_clock::time_point endTime = high_resolution_clock::now();
        duration<double, std::milli> time = endTime - startTime;

        cout << "iteration " << i << " time: " << time.count() << endl << endl;

        averageTime += time.count();
        durations.push_back(time.count());
    }

    long result = (long)((double) averageTime / iterationCount);

    cout << "average time for jobs [";
    printVector(jobIds, *&cout);
    cout << "] is " << result << endl << endl;
    return durations;
}

int main() {
    srand(unsigned(time(0)));
    oneapi::tbb::global_control global_limit(oneapi::tbb::global_control::max_allowed_parallelism, coreNumbersVector.size());
    PinningObserver p;
    int optionNumber = 0;
    for (int jobsCount : jobsCountArray) {
        for (int option = 0; option < optionsCount; option++) {
            vector<Job*> jobs(jobsCount);
            for (int j = 0; j < jobsCount; j++) {
                jobs[j] = generateJob();
            }

            for (string partialOrderType : partialOrderTypes) {
                cout << endl << "Starting execution for " << partialOrderType << " type" << endl;
                vector<vector<int>> orderTable = getOrderTable(jobs.size(), partialOrderType);
                vector<double> durations = run(jobs, orderTable);
                printData(durations, orderTable, partialOrderType, jobs, optionNumber);
            }

            for (int j = 0; j < jobsCount; j++) {
                delete jobs[j];
            }
            optionNumber++;
        }
    }

    cout << "execution completed" << endl;

    return 0;
}
