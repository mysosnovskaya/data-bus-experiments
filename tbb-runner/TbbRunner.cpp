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
#include <tbb/flow_graph.h>

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

vector<string> partialOrderTypes = { "NO_ORDER", "RANDOM", "BITREE", "ONE_TO_MANY_TO_ONE" };
vector<int> jobsCountArray = { 5, 10, 20, 40 };
int optionsCount = 10;
const int iterationCount = 7;

string getFileName(string partialOrderType, vector<Job*> jobs) {
    auto jobsCountStr = to_string(jobs.size());

    string jobsIdsString;
    for (int i = 0; i < jobs.size(); i++) {
        jobsIdsString = jobsIdsString + "_" + jobs[i]->getJobId();
    }
    return string("results/data_") + string(jobsCountStr) + "j_" + partialOrderType + "_" + jobsIdsString + string(".txt");
}

void printData(vector<double> durations, vector<vector<int>> orderTable, string partialOrderType, vector<Job*> jobs) {
    string fileName = getFileName(partialOrderType, jobs);

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
                printData(durations, orderTable, partialOrderType, jobs);
            }

            for (int j = 0; j < jobsCount; j++) {
                delete jobs[j];
            }
        }
    }

    cout << "execution completed" << endl;

    return 0;
}
