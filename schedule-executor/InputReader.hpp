#pragma once

using namespace std;

vector<double> delayis;
vector<Job*> jobs;
vector<vector<int>> order;
vector<thread> threads;
vector<bool> jobIsFinished;
vector<vector<int>> queue;

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
