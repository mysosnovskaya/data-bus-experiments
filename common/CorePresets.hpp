#pragma once

#include <vector>
#include <iostream>
#include <stdexcept>

vector<int> INTEL_SERVER_NODE_0 = {
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19
};

vector<int> INTEL_SERVER_NODE_1 = {
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37, 38, 39
};

vector<int> HUAWEI_SERVER_NODE_0 = {
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,
     12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
};

vector<int> HUAWEI_SERVER_NODE_1 = {
    24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
    36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47
};

vector<int> getCoreNumbers(string presetName) {
    if (presetName == "INTEL_SERVER_NODE_0") {
        return INTEL_SERVER_NODE_0;
    } else if (presetName == "INTEL_SERVER_NODE_1") {
        return INTEL_SERVER_NODE_1;
    } else if (presetName == "HUAWEI_SERVER_NODE_0") {
       return HUAWEI_SERVER_NODE_0;
    } else if (presetName == "HUAWEI_SERVER_NODE_1") {
       return HUAWEI_SERVER_NODE_1;
    } else {
        cerr << "Unknown core preset name " << presetName << endl;
        throw invalid_argument("Unknown core preset name " + presetName);
    }
}