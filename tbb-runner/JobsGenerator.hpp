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

vector<string> jobTypes = { "XPY", "SUM", "QR", "COPY", "MUL" };

vector<int> xpyPossibleSizes = { 20, 35, 50, 65 };
vector<int> sumPossibleSizes = { 200, 350, 500, 650 };
vector<int> qrPossibleSizes = { 1000, 1100, 1200, 1300 };
vector<int> copyPossibleSizes = { 10, 25, 40, 55 };
vector<int> mulPossibleSizes = { 1000, 1100, 1200, 1300 };

Job* generateJob() {
    string type = jobTypes[randInt(0, jobTypes.size())];
    int size;
    Job* job;
    if (type == "SUM") {
        size = sumPossibleSizes[randInt(0, sumPossibleSizes.size())];
        job = MklSumJob::create(size);
    }
    else if (type == "COPY") {
        size = copyPossibleSizes[randInt(0, copyPossibleSizes.size())];
        job = MklCopyJob::create(size);
    }
    else if (type == "XPY") {
        size = xpyPossibleSizes[randInt(0, xpyPossibleSizes.size())];
        job = MklXpyJob::create(size);
    }
    else if (type == "MUL") {
        size = mulPossibleSizes[randInt(0, mulPossibleSizes.size())];
        job = MklMulJob::create(size);
    }
    else {
        size = qrPossibleSizes[randInt(0, qrPossibleSizes.size())];
        job = MklQrJob::create(size);
    }
    return job;
}
