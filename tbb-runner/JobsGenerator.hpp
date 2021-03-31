#pragma once

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>
#include <random>
#include <set>
#include <string>
#include "../common/Jobs.hpp"

using namespace std;

vector<string> jobTypes = { "XPY", "SUM", "QR", "COPY" };

const int minSumSize = 30;
const int minXpySize = 20;
const int minCopySize = 10;
const int minQrSize = 1000;

const int sumInterval = 5;
const int xpyInterval = 5;
const int copyInterval = 5;
const int qrInterval = 50;

const int maxSumIntervalCount = 8;
const int maxXpyIntervalCount = 8;
const int maxCopyIntervalCount = 8;
const int maxQrIntervalCount = 6;

Job* generateJob() {
    string type = jobTypes[randInt(0, jobTypes.size())];
    int size;
    Job* job;
    if (type == "SUM") {
        size = minSumSize + randInt(0, maxSumIntervalCount + 1) * sumInterval;
        job = MklSumJob::create(size);
    }
    else if (type == "COPY") {
        size = minCopySize + randInt(0, maxCopyIntervalCount + 1) * copyInterval;
        job = MklCopyJob::create(size);
    }
    else if (type == "XPY") {
        size = minXpySize + randInt(0, maxXpyIntervalCount + 1) * xpyInterval;
        job = MklXpyJob::create(size);
    }
    else {
        size = minQrSize + randInt(0, maxQrIntervalCount + 1) * qrInterval;
        job = MklQrJob::create(size);
    }
    return job;
}