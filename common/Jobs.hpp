#pragma once

#include <chrono>
#include <time.h>
#include <string>
#ifdef MKL_ILP64
#include "mkl.h"
#define USE_MKL
#else
#include <cblas64.h>
#include <lapacke.h>
#define mkl_malloc(sz, al) malloc(sz)
#define mkl_free free
#endif
#include "ExecutionFlag.hpp"

using namespace std;
using namespace std::chrono;

class Job {
public:
    virtual int getSize() = 0;

    virtual string getType() = 0;

    string getJobId() {
        auto jobsSizeStr = to_string(getSize());
        return jobsSizeStr + string("_") + getType();
    }

    virtual int execute(double* percentOfExecution, bool changeFlagTo) = 0;

    int execute(int indexJob) {
        printf("job %d started \n", indexJob);
        high_resolution_clock::time_point startTime = high_resolution_clock::now();
        double tmp;
        int result = execute(&tmp, false);
        high_resolution_clock::time_point endTime = high_resolution_clock::now();
        duration<double, std::milli> time = endTime - startTime;
        printf("job %d finished for %f ms\n", indexJob, time.count());
        return result;
    }

    virtual Job* copy() = 0;

    virtual ~Job() {};
};


class MklMulJob : public Job {
private:
    double* x;
    double* y;
    double* z;

public:
    int size;
    MklMulJob(int size, double* x, double* y, double* z) :
        size(size),
        x(x),
        y(y),
        z(z) {}

    static MklMulJob* create(int size) {
        double* x = (double*)mkl_malloc(size * size * sizeof(double), 64);
        double* y = (double*)mkl_malloc(size * size * sizeof(double), 64);
        double* z = (double*)mkl_malloc(size * size * sizeof(double), 64);

        for (int i = 0; i < size * size; i++) {
            x[i] = (double) i;
            y[i] = (double) i + 1;
            z[i] = (double) i + 2;
        }

        return new MklMulJob(size, x, y, z);
    }

    int execute(double* percentOfExecution, bool changeFlagTo) {
        for (int i = 0; i < 100; i++) {
            if (GLOBAL_EXECUTION_FLAG) {
                *percentOfExecution = (double)i / 100;
                return i;
            }
            cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                size, size, size, 1.0, x, size, y, size, 0.0, z, size);
        }
        GLOBAL_EXECUTION_FLAG = changeFlagTo;
        *percentOfExecution = 1.0;
        return 100;
    }

    int getSize() {
        return size;
    }

    string getType() {
        return "MUL";
    }

    Job* copy() {
         return MklMulJob::create(getSize());
    }

    ~MklMulJob() {
        mkl_free(x);
        mkl_free(y);
        mkl_free(z);
    }
};

class MklCopyJob : public Job {
private:
    double* x;
    double* y;

public:
    int size;
    int count = 0;
    MklCopyJob(int size, double* x, double* y) :
        size(size),
        x(x),
        y(y) {}

    MklCopyJob(const MklCopyJob &job) {x = job.x; y = job.y; }

    static MklCopyJob* create(int size) {
        long scaledSize = size * 1000000;
        double* x = (double*)mkl_malloc(scaledSize * sizeof(double), 64);
        double* y = (double*)mkl_malloc(scaledSize * sizeof(double), 64);

        for (int i = 0; i < scaledSize; i++) {
            x[i] = (double) i + 3;
            y[i] = (double) i + 4;
        }

        return new MklCopyJob(scaledSize, x, y);
    }

    int execute(double* percentOfExecution, bool changeFlagTo) {
        for (int i = 0; i < 100; i++) {
            if (GLOBAL_EXECUTION_FLAG) {
                *percentOfExecution = (double)i / 100;
                return i;
            }
            if (count % 2 == 0) {
                cblas_dcopy(size, x, 1, y, 1);
            }
            else {
                cblas_dcopy(size, y, 1, x, 1);
            }
        }
        GLOBAL_EXECUTION_FLAG = changeFlagTo;
        *percentOfExecution = 1.0;
        count++;
        return 100;
    }

    int getSize() {
        return size / 1000000;
    }

    string getType() {
        return "COPY";
    }

    Job* copy() {
         return MklCopyJob::create(getSize());
    }

    ~MklCopyJob() {
        mkl_free(x);
        mkl_free(y);
    }
};

class MklQrJob : public Job {
private:
    double* x;
    double* y;

public:
    int size;
    MklQrJob(int size, double* x, double* y) :
        size(size),
        x(x),
        y(y) {}

    MklQrJob(const MklQrJob &job) {x = job.x; y = job.y; }

    static MklQrJob* create(int size) {
        double* x = (double*)mkl_malloc(size * size * sizeof(double), 64);
        double* y = (double*)mkl_malloc(size * sizeof(double), 64);

        for (int i = 0; i < size * size; i++) {
            x[i] = (double) i + 5;
        }

        for (int i = 0; i < size; i++) {
            y[i] = (double) i + 6;
        }

        return new MklQrJob(size, x, y);
    }

    int execute(double* percentOfExecution, bool changeFlagTo) {
        for (int i = 0; i < 100; i++) {
            if (GLOBAL_EXECUTION_FLAG) {
                *percentOfExecution = (double)i / 100;
                return i;
            }
            LAPACKE_dgeqrf(LAPACK_ROW_MAJOR, size, size, x, size, y);
        }
        GLOBAL_EXECUTION_FLAG = changeFlagTo;
        *percentOfExecution = 1.0;
        return 100;
    }

    int getSize() {
        return size;
    }

    string getType() {
        return "QR";
    }

    Job* copy() {
         return MklQrJob::create(getSize());
    }

    ~MklQrJob() {
        mkl_free(x);
        mkl_free(y);
    }
};

class MklSumJob : public Job {
private:
    double* x;

public:
    int size;
    MklSumJob(int size, double* x) :
        size(size),
        x(x) {}

    MklSumJob(const MklSumJob &job) {x = job.x; }

    static MklSumJob* create(int size) {
        long scaledSize = size * 1000000;
        double* x = (double*)mkl_malloc(scaledSize * sizeof(double), 64);

        for (int i = 0; i < scaledSize; i++) {
            x[i] = (double) i + 7;
        }

        return new MklSumJob(scaledSize, x);
    }

    int execute(double* percentOfExecution, bool changeFlagTo) {
        for (int i = 0; i < 100; i++) {
            if (GLOBAL_EXECUTION_FLAG) {
                *percentOfExecution = (double)i / 100;
                return i;
            }
            // Computes the sum of magnitudes of the vector elements.
            cblas_dasum(size, x, 1);
        }
        GLOBAL_EXECUTION_FLAG = changeFlagTo;
        *percentOfExecution = 1.0;
        return 100;
    }

    int getSize() {
        return size / 1000000;
    }

    string getType() {
        return "SUM";
    }

    Job* copy() {
         return MklSumJob::create(getSize());
    }

    ~MklSumJob() {
        mkl_free(x);
    }
};

class MklXpyJob : public Job {
private:
    double* x;
    double* y;

public:
    int size;
    MklXpyJob(int size, double* x, double* y) :
        size(size),
        x(x),
        y(y) {}

    MklXpyJob(const MklXpyJob &job) {x = job.x; y = job.y; }

    static MklXpyJob* create(int size) {
        long scaledSize = size * 1000000;
        double* x = (double*)mkl_malloc(scaledSize * sizeof(double), 64);
        double* y = (double*)mkl_malloc(scaledSize * sizeof(double), 64);

        for (int i = 0; i < scaledSize; i++) {
            x[i] = (double) i + 8;
            y[i] = (double) i + 9;
        }

        return new MklXpyJob(scaledSize, x, y);
    }

    int execute(double* percentOfExecution, bool changeFlagTo) {
        for (int i = 0; i < 100; i++) {
            if (GLOBAL_EXECUTION_FLAG) {
                *percentOfExecution = (double)i / 100;
                return i;
            }
            //Computes a vector-scalar product and adds the result to a vector. y := a*x + y
            cblas_daxpy(size, 3.5, x, 1, y, 1);
        }
        GLOBAL_EXECUTION_FLAG = changeFlagTo;
        *percentOfExecution = 1.0;
        return 100;
    }

    int getSize() {
        return size / 1000000;
    }

    string getType() {
        return "XPY";
    }

    Job* copy() {
         return MklXpyJob::create(getSize());
    }

    ~MklXpyJob() {
        mkl_free(x);
        mkl_free(y);
    }
};
