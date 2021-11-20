#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <cstring>
#include "../common/Utils.hpp"

using namespace std;

vector<string> jobTypes = { "XPY", "SUM", /*"QR",*/ "COPY", "MUL" };
vector<string> partialOrderTypes = { "NO_ORDER", "RANDOM", "BITREE", "ONE_TO_MANY_TO_ONE" };

vector<int> xpyPossibleSizes = { 20, 35, 50, 65 };
vector<int> sumPossibleSizes = { 200, 350, 500, 650 };
vector<int> qrPossibleSizes = { 1000, 1100, 1200, 1300 };
vector<int> copyPossibleSizes = { 10, 25, 40, 55 };
vector<int> mulPossibleSizes = { 1000, 1100, 1200, 1300 };

double EDGE_PROBABILITY_DEFAULT = 0.02;
int AVG_GRAPH_COMPONENT_SIZE_DEFAULT = 10;

string generateJobId() {
    string type = jobTypes[randInt(0, jobTypes.size())];
    int size;
    if (type == "SUM") {
        size = sumPossibleSizes[randInt(0, sumPossibleSizes.size())];
    } else if (type == "COPY") {
        size = copyPossibleSizes[randInt(0, copyPossibleSizes.size())];
    } else if (type == "XPY") {
        size = xpyPossibleSizes[randInt(0, xpyPossibleSizes.size())];
    } else if (type == "MUL") {
        size = mulPossibleSizes[randInt(0, mulPossibleSizes.size())];
    } else {
        size = qrPossibleSizes[randInt(0, qrPossibleSizes.size())];
    }
    auto jobsSizeStr = to_string(size);
    return jobsSizeStr + string(" ") + type;
}

void prepareToPartialOrder(vector<int>* partialOrder, vector<int>* graphComponentsSeparatorsList, int jobsCount, int avgGraphComponentSize) {
    int maxGraphComponentsCount = jobsCount / avgGraphComponentSize;

    int graphComponentsCount = (int) (randBinary() * maxGraphComponentsCount + 1);

    set<int> graphComponentsSeparators;

    graphComponentsSeparators.insert(0);
    graphComponentsSeparators.insert(jobsCount - 1);

    while (graphComponentsSeparators.size() != ((long long) (graphComponentsCount + 1))) {
        graphComponentsSeparators.insert(randInt(0, jobsCount));
    }

    for (int i = 0; i < jobsCount; i++) {
        partialOrder->push_back(i);
    }

    auto rng = default_random_engine{};
    shuffle(partialOrder->begin(), partialOrder->end(), rng);

    for (auto f : graphComponentsSeparators) {
        graphComponentsSeparatorsList->push_back(f);
    }

    sort(graphComponentsSeparatorsList->begin(), graphComponentsSeparatorsList->end());
}

vector<vector<int>> getRandomOrderTable(int jobsCount, double edgeProbability) {
    vector<vector<int>> orderTable(jobsCount);

    for (int i = 0; i < jobsCount; i++) {
        orderTable[i].resize(jobsCount);
    }

    for (int i = 0; i < jobsCount; i++) {
        for (int j = i + 1; j < jobsCount; j++) {
            if ( ((double) rand() / RAND_MAX) <= edgeProbability) {
                orderTable[i][j] = 1;
            }
        }
    }

    return orderTable;
}

vector<vector<int>> getBiTreeOrderTable(int jobsCount, int avgGraphComponentSize) {
    vector<int> graphComponentsSeparatorsList;
    vector<int> order;

    prepareToPartialOrder(&order, &graphComponentsSeparatorsList, jobsCount, avgGraphComponentSize);

    vector<vector<int>> orderTable(jobsCount);

    for (int i = 0; i < jobsCount; i++) {
        orderTable[i].resize(jobsCount);
    }

    for (int i = 1; i < graphComponentsSeparatorsList.size(); i++) {
        int endIndex = graphComponentsSeparatorsList[i] == jobsCount - 1 ? graphComponentsSeparatorsList[i] + 1 : graphComponentsSeparatorsList[i];

        vector<int> tree;

        for (int ind = graphComponentsSeparatorsList[(long long) (i - 1)]; ind < endIndex; ind++) {
            int& tmp = order[ind];
            tree.push_back(tmp);
        }

        int parentIndex = 0;
        int childIndex = 1;

        while (childIndex < tree.size()) {
            orderTable[tree[childIndex]][tree[parentIndex]] = 1;
            childIndex++;
            if (childIndex >= tree.size()) {
                break;
            }
            orderTable[tree[childIndex]][tree[parentIndex]] = 1;
            parentIndex++;
            childIndex++;
        }
    }

    return orderTable;
}

vector<vector<int>> getOneToManyToOneOrderTable(int jobsCount, int avgGraphComponentSize) {
    vector<int> graphComponentsSeparatorsList;
    vector<int> order;

    prepareToPartialOrder(&order, &graphComponentsSeparatorsList, jobsCount, avgGraphComponentSize);

    vector<vector<int>> orderTable(jobsCount);

    for (int i = 0; i < jobsCount; i++) {
        orderTable[i].resize(jobsCount);
    }

    for (int i = 1; i < graphComponentsSeparatorsList.size(); i++) {
        int endIndex = graphComponentsSeparatorsList[i] == jobsCount - 1 ? graphComponentsSeparatorsList[i] + 1 : graphComponentsSeparatorsList[i];

        vector<int> otmto;

        for (int ind = graphComponentsSeparatorsList[(long long) (i - 1)]; ind < endIndex; ind++) {
            int& tmp = order[ind];
            otmto.push_back(tmp);
        }

        int startJobIndex = otmto[0];
        int endJobIndex = otmto[otmto.size() - 1];

        if (otmto.size() == 2) {
            orderTable[startJobIndex][endJobIndex] = 1;
        } else {
            for (int j = 1; j < otmto.size() - 1; j++) {
                orderTable[otmto[j]][startJobIndex] = 1;
                orderTable[endJobIndex][otmto[j]] = 1;
            }
        }
    }

    return orderTable;
}

vector<vector<int>> getOrderTable(int jobsCount, string partialOrderType, double edgeProbability, int avgGraphComponentSize) {
    vector<vector<int>> orderTable(jobsCount);
    if (partialOrderType == "NO_ORDER") {
        for (int i = 0; i < jobsCount; i++) {
            orderTable[i].resize(jobsCount);
        }
    }
    else if (partialOrderType == "BITREE") {
        orderTable = getBiTreeOrderTable(jobsCount, avgGraphComponentSize);
    }
    else if (partialOrderType == "RANDOM") {
        orderTable = getRandomOrderTable(jobsCount, edgeProbability);
    }
    else if (partialOrderType == "ONE_TO_MANY_TO_ONE") {
        orderTable = getOneToManyToOneOrderTable(jobsCount, avgGraphComponentSize);
    }

    return orderTable;
}

string getFileName(string directory, string partialOrderType, int jobsCount, int optionNumber) {
    auto jobsCountStr = to_string(jobsCount);
    auto optionNumberStr = to_string(optionNumber);

    return directory + string("/example_") + string(jobsCountStr) + "j_" + partialOrderType + "_" + optionNumberStr + string(".txt");
}

void printData(string outputDirectory, vector<vector<int>> orderTable, string partialOrderType, vector<string> jobIds, int optionNumber) {
    string fileName = getFileName(outputDirectory, partialOrderType, jobIds.size(), optionNumber);

    ofstream myfile;
    myfile.open(fileName);

    myfile << jobIds.size() << endl;
    for (int i = 0; i < jobIds.size(); i++) {
        myfile << jobIds[i] << "  ";
    }
    myfile << endl;
    printVectorOfVectors(orderTable, *&myfile);
    myfile.close();
}

int main(int argc, char** argv) {
    srand(unsigned(time(0)));

    // required arguments
    int jobsCount = -1;
    int examplesCount = -1;

    // optional arguments
    string partialOrderType = "";
    double edgeProbability = EDGE_PROBABILITY_DEFAULT;
    int avgGraphComponentSize = AVG_GRAPH_COMPONENT_SIZE_DEFAULT;
    string outputDirectory = "jobs-generator/results";

    for (auto i = 0; i < argc; ++i) {
        auto name = argv[i];

        if (strcmp("--jobs-count", name) == 0) {
            ++i;
            if (i < argc) {
                auto value = argv[i];
                jobsCount = atoi(value);
            }
        }

        if (strcmp("--examples-count", name) == 0) {
            ++i;
            if (i < argc) {
                auto value = argv[i];
                examplesCount = atoi(value);
            }
        }

        if (strcmp("--partial-order-type", name) == 0) {
            ++i;
            if (i < argc) {
                auto value = argv[i];
                partialOrderType = value;
                if (find(partialOrderTypes.begin(), partialOrderTypes.end(), partialOrderType) == partialOrderTypes.end()) {
                    cerr << "Unknown partial order type " << partialOrderType << endl;
                    return -1;
                }
            }
        }

        if (strcmp("--output-dir", name) == 0) {
            ++i;
            if (i < argc) {
                auto value = argv[i];
                outputDirectory = value;
            }
        }

        if (strcmp("--edge-probability", name) == 0) {
            ++i;
            if (i < argc) {
                auto value = argv[i];
                edgeProbability = atof(value);
            }
        }

        if (strcmp("--avg-graph-component-size", name) == 0) {
            ++i;
            if (i < argc) {
                auto value = argv[i];
                avgGraphComponentSize = atoi(value);
            }
        }
    }

    if (jobsCount == -1) {
        cerr << "Missing jobs-count. See README.md for more information" << endl;
        return -1;
    }
    if (examplesCount == -1) {
        cerr << "Missing examples-count. See README.md for more information" << endl;
        return -1;
    }

    for (int i = 0; i < examplesCount; i++) {
        cout << "generating example " << i + 1 << "/" << examplesCount << endl;
        vector<string> jobIds = vector<string>();
        for (int j = 0; j < jobsCount; j++) {
            jobIds.push_back(generateJobId());
        }

        string curPartialOrderType = partialOrderType == "" ? partialOrderTypes[randInt(0, partialOrderTypes.size())] : partialOrderType;
        vector<vector<int>> orderTable = getOrderTable(jobsCount, curPartialOrderType, edgeProbability, avgGraphComponentSize);
        printData(outputDirectory, orderTable, curPartialOrderType, jobIds, i);
    }
    return 0;
}
