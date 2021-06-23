#pragma once

using namespace std;

vector<double> delays;
vector<Job*> jobs;
vector<vector<int>> order;
vector<thread> threads;
vector<bool> jobIsFinished;
vector<vector<int>> queue;

void readOrderTable(ifstream* inFile, int jobsCount) {
    cerr << "started execution of readOrderTable" << endl;
    order = vector<vector<int>>(jobsCount);

    for (int i = 0; i < jobsCount; i++) {
        order[i] = vector<int>(jobsCount);
    }

    for (int i = 0; i < jobsCount; i++) {
        for (int j = 0; j < jobsCount; j++) {
            *inFile >> order[i][j];
        }
    }
    cerr << "completed execution of readOrderTable" << endl;
}

void readDelays(ifstream* inFile, int jobsCount) {
    cerr << "started execution of readDelays" << endl;
    delays = vector<double>(jobsCount);

    for (int i = 0; i < jobsCount; i++) {
        *inFile >> delays[i];
    }
    cerr << "completed execution of readDelays" << endl;
}

void readJobs(ifstream* inFile, int jobsCount) {
    cerr << "started execution of readJobs" << endl;
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
    cerr << "completed execution of readJobs" << endl;
}

void readQueues(ifstream* inFile) {
    cerr << "started execution of readQueues" << endl;
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
    cerr << "completed execution of readQueues" << endl;
}
