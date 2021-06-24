#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include "../common/Jobs.hpp"

using namespace std;
using namespace std::chrono;

template<typename T>
void printVector(vector<T>& v, ostream& out) {
    for (int i = 0; i < v.size(); ++i) {
        out << v[i];
        if (i < v.size() - 1) {
            out << ",";
        }
    }
}

const int iterationCount = 3005;

struct JobFields {
    string type;
    int size;
};

vector<JobFields> jobs = {
   // {"XPY", 20}, {"XPY", 25}, {"XPY", 30}, {"XPY", 35}, {"XPY", 40}, {"XPY", 45}, {"XPY", 50}, {"XPY", 55}, {"XPY", 60}, {"XPY", 65},
  //  {"SUM", 200}, {"SUM", 250}, {"SUM", 300}, {"SUM", 350}, {"SUM", 400}, {"SUM", 450}, {"SUM", 500}, {"SUM", 550}, {"SUM", 600}, {"SUM", 650},
  //  {"QR", 1000}, {"QR", 1100}, {"QR", 1200}, {"QR", 1300}, {"QR", 1400}, {"QR", 1500}, {"QR", 1600}, {"QR", 1700}, {"QR", 1800}, {"QR", 1900},
 //   {"COPY", 10}, {"COPY", 20}, {"COPY", 30}, {"COPY", 40}, {"COPY", 50}, {"COPY", 60}, {"COPY", 70}, {"COPY", 80}, {"COPY", 90}, {"COPY", 100}
    {"COPY", 25}, {"COPY", 55}
};

string getFileName(string jobType) {
    return string("results/dispersion_collector_out_") + string(jobType) + string(".txt");
}

void printData(vector<double> durationsOfIterations, string jobType) {
    string fileName = getFileName(jobType);

    ofstream myfile;
    myfile.open(fileName);

    myfile << "Job " << jobType << endl;
    printVector(durationsOfIterations, *&myfile);
    myfile << endl;

    myfile.close();
}

long run(Job* job) {
    cout << "starting execution jobs " << job->getJobId() << endl;

    int average = 0;

    vector<double> jobTime(iterationCount);

    for (int i = 0; i < iterationCount; i++) {
        double tmp = 1.0;
        high_resolution_clock::time_point startTime = high_resolution_clock::now();

        job->execute(&tmp, false);

        high_resolution_clock::time_point endTime = high_resolution_clock::now();
        duration<double, std::milli> time = endTime - startTime;

        cout << "iteration " << i << " time: " << time.count() << endl;

        jobTime[i] = time.count();

        average += time.count();
    }

    printData(jobTime, job->getJobId());

    long result = (long)((double)average / iterationCount);

    cout << "average time for job [" << job->getJobId() << "] is " << result << endl << endl;
    return result;
}

int main() {
    srand(unsigned(time(0)));

    for (int i = 0; i < jobs.size(); i++) {
        string type = jobs[i].type;
        Job* job;
        if (type == "COPY") {
            job = MklCopyJob::create(jobs[i].size);
        }
        else if (type == "QR") {
            job = MklQrJob::create(jobs[i].size);
        }
        else if (type == "SUM") {
            job = MklSumJob::create(jobs[i].size);
        }
        else {
            job = MklXpyJob::create(jobs[i].size);
        }
        run(job);
        delete job;
    }

    cout << "execution completed" << endl;

    return 0;
}
