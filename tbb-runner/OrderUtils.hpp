#pragma once

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>
#include <random>
#include <set>
#include <string>
#include <algorithm>

using namespace std;

int PARTIAL_ORDER_COEFFICIENT = 2;
int RANDOM_PARTIAL_ORDER_COEFFICIENT = 5;

double randBinary() {
    return (double)(rand() % 100) / 100;
}

int randInt(int a, int b) {
    return randBinary() * ((long long)b - a) + a;
}

void printOrderTable(vector<vector<int>> orderTable, ostream& out) {
    out << "order: " << endl;
    for (int i = 0; i < orderTable.size(); i++) {
        for (int j = 0; j < orderTable[i].size(); j++) {
            if (orderTable[i][j] == 1) {
                out << j << " -> " << i << endl;
            }
        }
    }
    out << endl;
}

void prepareToPartialOrder(vector<int>* partialOrder, vector<int>* chainSeparatorsList, int jobsCount) {
    int maxChainCount = jobsCount / PARTIAL_ORDER_COEFFICIENT;

    int chainCount = (int)(randBinary() * maxChainCount + 1);

    cout << "count of partial order chains is: " << chainCount << endl;

    set<int> chainSeparators;

    chainSeparators.insert(0);
    chainSeparators.insert(jobsCount - 1);

    while (chainSeparators.size() != ((long long)(chainCount + 1))) {
        chainSeparators.insert(randInt(0, jobsCount));
    }

    for (int i = 0; i < jobsCount; i++) {
        partialOrder->push_back(i);
    }

    auto rng = default_random_engine{};
    shuffle(partialOrder->begin(), partialOrder->end(), rng);

    for (auto f : chainSeparators) {
        chainSeparatorsList->push_back(f);
    }

    sort(chainSeparatorsList->begin(), chainSeparatorsList->end());
}

vector<vector<int>> getRandomOrderTable(int jobsCount) {
    vector<vector<int>> orderTable(jobsCount);

    for (int i = 0; i < jobsCount; i++) {
        orderTable[i].resize(jobsCount);
    }

    for (int i = 0; i < jobsCount; i++) {
        for (int j = i + 1; j < jobsCount; j++) {
            if (rand() % RANDOM_PARTIAL_ORDER_COEFFICIENT == 0) {
                orderTable[i][j] = 1;
            }
        }
    }

    return orderTable;
}

vector<vector<int>> getBiTreeOrderTable(int jobsCount) {
    vector<int> chainSeparatorsList;
    vector<int> order;

    prepareToPartialOrder(&order, &chainSeparatorsList, jobsCount);

    vector<vector<int>> orderTable(jobsCount);

    for (int i = 0; i < jobsCount; i++) {
        orderTable[i].resize(jobsCount);
    }

    for (int i = 1; i < chainSeparatorsList.size(); i++) {
        int endIndex = chainSeparatorsList[i] == jobsCount - 1 ? chainSeparatorsList[i] + 1 : chainSeparatorsList[i];

        vector<int> tree;

        for (int ind = chainSeparatorsList[(long long)(i - 1)]; ind < endIndex; ind++) {
            int& tmp = order[ind];
            tree.push_back(tmp);
        }

        cout << "tree " << i << " has size " << tree.size() << " with root " << tree[0] << endl;

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

vector<vector<int>> getOneToManyToOneOrderTable(int jobsCount) {
    vector<int> chainSeparatorsList;
    vector<int> order;

    prepareToPartialOrder(&order, &chainSeparatorsList, jobsCount);

    vector<vector<int>> orderTable(jobsCount);

    for (int i = 0; i < jobsCount; i++) {
        orderTable[i].resize(jobsCount);
    }

    for (int i = 1; i < chainSeparatorsList.size(); i++) {
        int endIndex = chainSeparatorsList[i] == jobsCount - 1 ? chainSeparatorsList[i] + 1 : chainSeparatorsList[i];

        vector<int> otmto;

        for (int ind = chainSeparatorsList[(long long)(i - 1)]; ind < endIndex; ind++) {
            int& tmp = order[ind];
            otmto.push_back(tmp);
        }

        cout << "otmto " << i << " has size " << otmto.size() << " with root " << otmto[0] << " and end root " << otmto[otmto.size() - 1] << endl;

        int startJobIndex = otmto[0];
        int endJobIndex = otmto[otmto.size() - 1];

        if (otmto.size() == 2) {
            orderTable[startJobIndex][endJobIndex] = 1;
        }
        else {
            for (int j = 1; j < otmto.size() - 1; j++) {
                orderTable[otmto[j]][startJobIndex] = 1;
                orderTable[endJobIndex][otmto[j]] = 1;
            }
        }
    }

    return orderTable;
}

vector<vector<int>> getOrderTable(int jobsCount, string partialOrderType) {
    vector<vector<int>> orderTable(jobsCount);
    if (partialOrderType == "NO_ORDER") {
        for (int i = 0; i < jobsCount; i++) {
            orderTable[i].resize(jobsCount);
        }
    }
    else if (partialOrderType == "BITREE") {
        orderTable = getBiTreeOrderTable(jobsCount);
        printOrderTable(orderTable, *&cout);
    }
    else if (partialOrderType == "RANDOM") {
        orderTable = getRandomOrderTable(jobsCount);
        printOrderTable(orderTable, *&cout);
    }
    else if (partialOrderType == "ONE_TO_MANY_TO_ONE") {
        orderTable = getOneToManyToOneOrderTable(jobsCount);
        printOrderTable(orderTable, *&cout);
    }

    return orderTable;
}
