#pragma once

#include <vector>
#include <random>

using namespace std;

double randBinary() {
    return (double) (rand() % 100) / 100;
}

int randInt(int a, int b) {
    return randBinary() * ((long long) b - a) + a;
}

template<typename T>
void printVector(vector<T>& v, ostream& out) {
    for (int i = 0; i < v.size(); ++i) {
        out << v[i];
        if (i < v.size() - 1) {
            out << ",";
        }
    }
}

void printVectorOfVectors(vector<vector<int>> vectorOfVectors, ostream& out) {
    for (int i = 0; i < vectorOfVectors.size(); i++) {
        for (int j = 0; j < vectorOfVectors[i].size(); j++) {
            out << vectorOfVectors[i][j] << " ";
        }
        out << endl;
    }
    out << endl;
}
