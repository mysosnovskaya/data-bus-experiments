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

vector<Job*> jobs = {
    MklXpyJob::create(20), MklXpyJob::create(35), MklXpyJob::create(50), MklXpyJob::create(65),
    MklSumJob::create(200), MklSumJob::create(350), MklSumJob::create(500), MklSumJob::create(650),
    MklQrJob::create(1000), MklQrJob::create(1100), MklQrJob::create(1200), MklQrJob::create(1300),
    MklCopyJob::create(10), MklCopyJob::create(25), MklCopyJob::create(40), MklCopyJob::create(55)
};

void run() {
    std::this_thread::sleep_for(150ms);
    high_resolution_clock::time_point startTime = high_resolution_clock::now();
    for (Job* job : jobs) {
        duration<double, std::milli> timeFromStart = high_resolution_clock::now() - startTime;
        cout << timeFromStart.count() << ": execution job " << job->getJobId() << endl;
        high_resolution_clock::time_point startJobTime = high_resolution_clock::now();
        job->execute(0);
        high_resolution_clock::time_point endTime = high_resolution_clock::now();
        duration<double, std::milli> time = endTime - startJobTime;
        cout << "Job " << job->getJobId() << " execution time: " << time.count() << endl << endl;
    }
}

int main() {
    srand(unsigned(time(0)));
    thread t = thread(run);
    // next 4 lines are Linux only, comment it for Windows
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpuset);

    t.join();
    cout << "execution completed" << endl;
    return 0;
}
