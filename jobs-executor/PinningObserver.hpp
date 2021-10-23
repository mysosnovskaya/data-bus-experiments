#pragma once

#include <vector>
#include <tbb/tbb.h>
#include <iostream>
#include <sched.h>

class PinningObserver: public tbb::task_scheduler_observer {
private:
    vector<int> coreNumbersVector;
    tbb::concurrent_queue<int> coreNumbers;
public:
    PinningObserver(vector<int> coreNumbersVector) {
        this->coreNumbersVector = coreNumbersVector;
        coreNumbers = tbb::concurrent_queue<int>(coreNumbersVector.begin(), coreNumbersVector.end());
        observe(true);
    }

    void on_scheduler_entry(bool worker) {
        auto numberOfSlots = tbb::this_task_arena::max_concurrency();
        cpu_set_t *cpu_set = CPU_ALLOC(numberOfSlots);
        size_t setsize = CPU_ALLOC_SIZE(numberOfSlots);
        CPU_ZERO_S(setsize, cpu_set);
        int coreNumber = 0;
        coreNumbers.try_pop(coreNumber);
        CPU_SET_S(coreNumber, setsize, cpu_set);
        if (sched_setaffinity(0, setsize, cpu_set) < 0) {
            cerr << "Unable to Set Affinity" << endl;
        }
        CPU_FREE(cpu_set);
    }

    void on_scheduler_exit(bool worker) {
        auto numberOfSlots = tbb::this_task_arena::max_concurrency();
        cpu_set_t *cpu_set = CPU_ALLOC(numberOfSlots);
        size_t setsize = CPU_ALLOC_SIZE(numberOfSlots);
        CPU_ZERO_S(setsize, cpu_set);
        for (int coreNumber : coreNumbersVector) {
            CPU_SET_S(coreNumber, setsize, cpu_set);
        }
        if (sched_setaffinity(0, setsize, cpu_set) < 0) {
            cerr << "Unable to Set Affinity" << endl;
        }
        CPU_FREE(cpu_set);
    }
};