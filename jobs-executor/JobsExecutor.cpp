#include <dirent.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <time.h>
#include <cstdlib>
#include <algorithm>
#include <iostream>
#include <fstream>
#include "../common/Jobs.hpp"
#include "../common/Utils.hpp"
#include "../common/CorePresets.hpp"
#include "InputReader.hpp"
#include "PinningObserver.hpp"
#include <filesystem>
#include <sched.h>
#include <tbb/tbb.h>
#include <tbb/flow_graph.h>

using namespace std;
using namespace std::chrono;
namespace fs = std::filesystem;

const int ITERATION_COUNT_DEFAULT = 30;

long run(vector<Job*> jobs, vector<vector<int>>& order) {
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
    return time.count();
}

long execute(string inputFile) {
    ifstream inFile;
    inFile.open(inputFile);

    int jobsCount;
    inFile >> jobsCount;
    vector<Job*> jobs = readJobs(&inFile, jobsCount);
    vector<vector<int>> order = readOrderTable(&inFile, jobsCount);
    inFile.close();

    long iterationDuration = run(jobs, order);

    for (int i = 0; i < jobsCount; i++) {
        delete jobs[i];
    }

    return iterationDuration;
}

int main(int argc, char** argv) {
    srand(unsigned(time(0)));

    // required arguments
    string corePreset = "";

    // optional arguments
    string inputFile = "";
    string inputDir = "jobs-executor/input";
    int iterationCount = ITERATION_COUNT_DEFAULT;
    string outputFile = "jobs-executor/results/jobs_executor_results.txt";

    for (auto i = 0; i < argc; ++i) {
        auto name = argv[i];

        if (strcmp("--core-preset", name) == 0) {
            ++i;
            if (i < argc) {
                auto value = argv[i];
                corePreset = value;
            }
        }

        if (strcmp("--input-file", name) == 0) {
            ++i;
            if (i < argc) {
                auto value = argv[i];
                inputFile = value;
            }
        }

        if (strcmp("--input-dir", name) == 0) {
            ++i;
            if (i < argc) {
                auto value = argv[i];
                inputDir = value;
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

    oneapi::tbb::global_control global_limit(oneapi::tbb::global_control::max_allowed_parallelism, coresNumbers.size());
    PinningObserver p(coresNumbers);

    map<string, vector<long>> fileToIterationDurations;

    if (inputFile != "") {
        long iterationDuration = execute(inputFile);
        vector<long> iterationDurations;
        iterationDurations.push_back(iterationDuration);
        fileToIterationDurations[inputFile] = iterationDurations;
    } else {
        vector<string> filePaths;
        struct dirent *dir;
        DIR *d = opendir(inputDir.c_str());
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                string fileName(dir->d_name);
                if (fileName.find(".txt") == std::string::npos) {
                    continue;
                }
                string filePath = string(inputDir) + "/" + fileName;
                filePaths.push_back(filePath);
            }
            closedir(d);
        }

        for (int i = 0; i < filePaths.size(); ++i) {
            fileToIterationDurations[filePaths[i]] = {};
        }

        for (int it = 0; it < iterationCount; it++) {
            for (int i = 0; i < filePaths.size(); ++i) {
                cout << "Start execution iteration " << it + 1 << "/" << iterationCount << " for file " << filePaths[i] << " " << i + 1 << "/" << filePaths.size() << endl;
                long iterationDuration = execute(filePaths[i]);
                fileToIterationDurations[filePaths[i]].push_back(iterationDuration);
                cout << "time is " << iterationDuration << endl << endl;
            }
        }
    }

    ofstream myfile;
    myfile.open(outputFile, ios_base::app);
    for (auto entry : fileToIterationDurations) {
        myfile << entry.first << endl;
        printVector(entry.second, *&myfile);
        myfile << endl << endl;
    }
    myfile.close();

    return 0;
}
