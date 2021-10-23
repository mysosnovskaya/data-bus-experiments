#pragma once

using namespace std;

vector<vector<int>> readOrderTable(ifstream* inFile, int jobsCount) {
    vector<vector<int>> order = vector<vector<int>>(jobsCount);

    for (int i = 0; i < jobsCount; i++) {
        order[i] = vector<int>(jobsCount);
    }

    for (int i = 0; i < jobsCount; i++) {
        for (int j = 0; j < jobsCount; j++) {
            *inFile >> order[i][j];
        }
    }

    return order;
}

vector<Job*> readJobs(ifstream* inFile, int jobsCount) {
    vector<Job*> jobs = vector<Job*>();
    for (int i = 0; i < jobsCount; i++) {
        string jobType;
        int jobSize;
        *inFile >> jobSize;
        *inFile >> jobType;

        Job* job;
        if (jobType == "SUM") {
            job = MklSumJob::create(jobSize);
        } else if (jobType == "COPY") {
            job = MklCopyJob::create(jobSize);
        } else if (jobType == "XPY") {
            job = MklXpyJob::create(jobSize);
        } else if (jobType == "MUL") {
            job = MklMulJob::create(jobSize);
        } else {
            job = MklQrJob::create(jobSize);
        }
        jobs.push_back(job);
    }
    return jobs;
}
