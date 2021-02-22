#pragma once

#include <algorithm>
#include <vector>

using namespace std;

double calculateExpectedValue(vector<double> jobIterationsDurations) {
    double valuesSum = 0.0;
    for (int j = 0; j < jobIterationsDurations.size(); j++) {
        valuesSum += jobIterationsDurations[j];
    }
    return (double)valuesSum / jobIterationsDurations.size();
}

double calculateDispersion(vector<double> jobIterationsDurations, double expectedValue) {
    double valuesSquareSum = 0.0;
    for (int j = 0; j < jobIterationsDurations.size(); j++) {
        valuesSquareSum += (jobIterationsDurations[j] * jobIterationsDurations[j]);
    }
    return ((double)valuesSquareSum / jobIterationsDurations.size()) - expectedValue * expectedValue;
}

double calculateStandardDeviation(vector<double> jobIterationsDurations, double expectedValue) {
    double squareDeviationSum = 0.0;
    for (int j = 0; j < jobIterationsDurations.size(); j++) {
        squareDeviationSum += pow((jobIterationsDurations[j] - expectedValue), 2);
    }

    return sqrt(squareDeviationSum / (jobIterationsDurations.size() - 1));
}

double calculateMinValue(vector<double> jobIterationsDurations) {
    return jobIterationsDurations[distance(jobIterationsDurations.begin(), min_element(jobIterationsDurations.begin(), jobIterationsDurations.end()))];
}

double calculateMaxValue(vector<double> jobIterationsDurations) {
    return jobIterationsDurations[distance(jobIterationsDurations.begin(), max_element(jobIterationsDurations.begin(), jobIterationsDurations.end()))];
}