#pragma once

#include <chrono>
#include <string>
#include <iostream>
#include <time.h>
#ifdef MKL_ILP64
#include "mkl.h"
#define USE_MKL
#else
#include <cblas64.h>
#include <lapacke.h>
#include <mkl_vml_functions.h>
#include <cmath>
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
        printf("%lu job %s_%d started %ld\n", hash<std::thread::id>{}(this_thread::get_id()), getJobId().c_str(), indexJob, duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
        high_resolution_clock::time_point startTime = high_resolution_clock::now();
        double tmp;
        int result = execute(&tmp, false);
        high_resolution_clock::time_point endTime = high_resolution_clock::now();
        duration<double, std::milli> time = endTime - startTime;
        printf("%lu job %s_%d finished %ld for %f ms\n", hash<std::thread::id>{}(this_thread::get_id()), getJobId().c_str(), indexJob, duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count(), time.count());
        return result;
    }

    virtual Job* copy() = 0;

    virtual ~Job() {};
};

// ============================================================================
// ERF JOB (vdErf) — функция ошибок. Определена на всей прямой, область
// значений [-1, 1] — самая "безопасная" из VML-функций: никогда не уходит
// ни в Inf, ни в денормалы, при любом x.
// ============================================================================
class MklErfJob : public Job {
private:
    double* x;
    double* y;

public:
    int size;
    MklErfJob(int size, double* x, double* y) :
        size(size),
        x(x),
        y(y) {}

    MklErfJob(const MklErfJob &job) { x = job.x; y = job.y; }

    static MklErfJob* create(int size) {
        long scaledSize = size * 1000000;
        double* x = (double*)mkl_malloc(scaledSize * sizeof(double), 64);
        double* y = (double*)mkl_malloc(scaledSize * sizeof(double), 64);

        // x в [-3, 3) — до |x|>3 erf ещё не "прижался" к асимптотам ±1,
        // так что VML реально считает, а не возвращает готовую константу
        for (long i = 0; i < scaledSize; i++) {
            x[i] = (double)(i % 6000) * 0.001 - 3.0;
            y[i] = 0.0;
        }

        MklErfJob* job = new MklErfJob(scaledSize, x, y);

        cerr << job->getJobId() << "::x : " << x << endl;
        cerr << job->getJobId() << "::y : " << y << endl;

        return job;
    }

    int execute(double* percentOfExecution, bool changeFlagTo) {
        for (int i = 0; i < 100; i++) {
            vdErf(size, x, y);
        }
        *percentOfExecution = 1.0;
        return 100;
    }

    int getSize() { return size / 1000000; }
    string getType() { return "ERF"; }
    Job* copy() { return MklErfJob::create(getSize()); }

    ~MklErfJob() {
        mkl_free(x);
        mkl_free(y);
    }
};

// ============================================================================
// TGAMMA JOB (vdTGamma) — гамма-функция, самая тяжёлая по компьюту из трёх.
// Полюса в 0, -1, -2... и очень быстрый рост (переполнение double уже при
// x≈171). Держим x строго в [1, 6) — гладкая зона, без полюсов и переполнения
// (tgamma(6)=120, tgamma(1)=1).
// ============================================================================
class MklTgammaJob : public Job {
private:
    double* x;
    double* y;

public:
    int size;
    MklTgammaJob(int size, double* x, double* y) :
        size(size),
        x(x),
        y(y) {}

    MklTgammaJob(const MklTgammaJob &job) { x = job.x; y = job.y; }

    static MklTgammaJob* create(int size) {
        long scaledSize = size * 1000000;
        double* x = (double*)mkl_malloc(scaledSize * sizeof(double), 64);
        double* y = (double*)mkl_malloc(scaledSize * sizeof(double), 64);

        // x в [1, 6) — вдали от полюсов и от зоны переполнения
        for (long i = 0; i < scaledSize; i++) {
            x[i] = (double)(i % 5000) * 0.001 + 1.0;
            y[i] = 0.0;
        }

        MklTgammaJob* job = new MklTgammaJob(scaledSize, x, y);

        cerr << job->getJobId() << "::x : " << x << endl;
        cerr << job->getJobId() << "::y : " << y << endl;

        return job;
    }

    int execute(double* percentOfExecution, bool changeFlagTo) {
        for (int i = 0; i < 100; i++) {
            vdTGamma(size, x, y);
        }
        *percentOfExecution = 1.0;
        return 100;
    }

    int getSize() { return size / 1000000; }
    string getType() { return "TGAMMA"; }
    Job* copy() { return MklTgammaJob::create(getSize()); }

    ~MklTgammaJob() {
        mkl_free(x);
        mkl_free(y);
    }
};

// ============================================================================
// POW JOB (vdPow) — единственная бинарная функция набора: y[i] = a[i]^b[i].
// На входе ДВА вектора (a, b), а не один — трафик чтения вдвое больше, чем
// у EXP/LN/SQRT (не 8n, а 16n на чтение), это важно учесть при подсчёте шины.
// ============================================================================
class MklPowJob : public Job {
private:
    double* a; // основание
    double* b; // показатель степени
    double* y; // результат

public:
    int size;
    MklPowJob(int size, double* a, double* b, double* y) :
        size(size),
        a(a),
        b(b),
        y(y) {}

    MklPowJob(const MklPowJob &job) { a = job.a; b = job.b; y = job.y; }

    static MklPowJob* create(int size) {
        long scaledSize = size * 1000000;
        double* a = (double*)mkl_malloc(scaledSize * sizeof(double), 64);
        double* b = (double*)mkl_malloc(scaledSize * sizeof(double), 64);
        double* y = (double*)mkl_malloc(scaledSize * sizeof(double), 64);

        // a строго положительное (иначе дробная степень отрицательного
        // основания даёт NaN); оба множителя в [1, 2) → результат в [1, 4),
        // без риска переполнения/денормалов на всех 100 повторах
        for (long i = 0; i < scaledSize; i++) {
            a[i] = 1.0 + (double)(i % 1000) * 0.001;
            b[i] = 1.0 + (double)((i + 500) % 1000) * 0.001;
            y[i] = 0.0;
        }

        MklPowJob* job = new MklPowJob(scaledSize, a, b, y);

        cerr << job->getJobId() << "::a : " << a << endl;
        cerr << job->getJobId() << "::b : " << b << endl;
        cerr << job->getJobId() << "::y : " << y << endl;

        return job;
    }

    int execute(double* percentOfExecution, bool changeFlagTo) {
        for (int i = 0; i < 100; i++) {
            vdPow(size, a, b, y);
        }
        *percentOfExecution = 1.0;
        return 100;
    }

    int getSize() { return size / 1000000; }
    string getType() { return "POW"; }
    Job* copy() { return MklPowJob::create(getSize()); }

    ~MklPowJob() {
        mkl_free(a);
        mkl_free(b);
        mkl_free(y);
    }
};

class MklExpJob : public Job {
private:
    double* x;

public:
    int size;
    MklExpJob(int size, double* x) :
        size(size),
        x(x) {}

    MklExpJob(const MklExpJob &job) { x = job.x; }

    static MklExpJob* create(int size) {
        long scaledSize = size * 1000000;
        double* x = (double*)mkl_malloc(scaledSize * sizeof(double), 64);

        // Прогрев обоих массивов + безопасный домен для exp: x в [0, 1)
        for (long i = 0; i < scaledSize; i++) {
            x[i] = (double)(i % 1000) * 0.001;
        }

        MklExpJob* job = new MklExpJob(scaledSize, x);

        cerr << job->getJobId() << "::x : " << x << endl;

        return job;
    }

    int execute(double* percentOfExecution, bool changeFlagTo) {
   //     cerr << getJobId() << "::execute()" << endl;

        for (int i = 0; i < 100; i++) {
 //            if (GLOBAL_EXECUTION_FLAG) {
 //                *percentOfExecution = (double)i / 100;
 //                return i;
 //            }
            // y[i] = exp(x[i]); x не меняется между вызовами → домен стабилен
            vdExp(size, x, x);
        }
  //      GLOBAL_EXECUTION_FLAG = changeFlagTo;
        *percentOfExecution = 1.0;
        return 100;
    }

    int getSize() { return size / 1000000; }
    string getType() { return "EXPX"; }
    Job* copy() { return MklExpJob::create(getSize()); }

    ~MklExpJob() {
        mkl_free(x);
    }
};

class MklLnJob : public Job {
private:
    double* x;

public:
    int size;
    MklLnJob(int size, double* x) :
        size(size),
        x(x) {}

    MklLnJob(const MklLnJob &job) { x = job.x;}

    static MklLnJob* create(int size) {
        long scaledSize = size * 1000000;
        double* x = (double*)mkl_malloc(scaledSize * sizeof(double), 64);

        // sqrt требует x >= 0; заодно прогреваем память
        for (long i = 0; i < scaledSize; i++) {
            x[i] = (double)(i % 1000) + 1.0;
        }

        MklLnJob* job = new MklLnJob(scaledSize, x);

        cerr << job->getJobId() << "::x : " << x << endl;

        return job;
    }

    int execute(double* percentOfExecution, bool changeFlagTo) {
   //     cerr << getJobId() << "::execute()" << endl;

        for (int i = 0; i < 100; i++) {
 //            if (GLOBAL_EXECUTION_FLAG) {
 //                *percentOfExecution = (double)i / 100;
 //                return i;
 //            }
            // y[i] = exp(x[i]); x не меняется между вызовами → домен стабилен
            vdLn(size, x, x);
        }
  //      GLOBAL_EXECUTION_FLAG = changeFlagTo;
        *percentOfExecution = 1.0;
        return 100;
    }


    int getSize() { return size / 1000000; }
    string getType() { return "LNX"; }
    Job* copy() { return MklLnJob::create(getSize()); }

    ~MklLnJob() {
        mkl_free(x);
    }
};

class MklSqrtJob : public Job {
private:
    double* x;

public:
    int size;
    MklSqrtJob(int size, double* x) :
        size(size),
        x(x) {}

    MklSqrtJob(const MklSqrtJob &job) { x = job.x; }

    static MklSqrtJob* create(int size) {
        long scaledSize = size * 1000000;
        double* x = (double*)mkl_malloc(scaledSize * sizeof(double), 64);

        // sqrt требует x >= 0; заодно прогреваем память
        for (long i = 0; i < scaledSize; i++) {
            x[i] = (double)(i % 1000) + 1.0;
        }

        MklSqrtJob* job = new MklSqrtJob(scaledSize, x);

        cerr << job->getJobId() << "::x : " << x << endl;

        return job;
    }

    int execute(double* percentOfExecution, bool changeFlagTo) {
   //     cerr << getJobId() << "::execute()" << endl;

        for (int i = 0; i < 100; i++) {
 //            if (GLOBAL_EXECUTION_FLAG) {
 //                *percentOfExecution = (double)i / 100;
 //                return i;
 //            }
            // y[i] = exp(x[i]); x не меняется между вызовами → домен стабилен
            vdSqrt(size, x, x);
        }
  //      GLOBAL_EXECUTION_FLAG = changeFlagTo;
        *percentOfExecution = 1.0;
        return 100;
    }


    int getSize() { return size / 1000000; }
    string getType() { return "SQRTX"; }
    Job* copy() { return MklSqrtJob::create(getSize()); }

    ~MklSqrtJob() {
        mkl_free(x);
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

        MklCopyJob* job = new MklCopyJob(scaledSize, x, y);

        cerr << job->getJobId() << "::x : " << x << endl;
        cerr << job->getJobId() << "::y : " << y << endl;

        return job;
    }

    int execute(double* percentOfExecution, bool changeFlagTo) {
  //      cerr << getJobId() << "::execute()" << endl;

        for (int i = 0; i < 100; i++) {
  //          if (GLOBAL_EXECUTION_FLAG) {
  //              *percentOfExecution = (double)i / 100;
  //              return i;
  //          }
            if (count % 2 == 0) {
                cblas_dcopy(size, x, 1, y, 1);
            }
            else {
                cblas_dcopy(size, y, 1, x, 1);
            }
        }
    //    GLOBAL_EXECUTION_FLAG = changeFlagTo;
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

        MklSumJob* job = new MklSumJob(scaledSize, x);

        cerr << job->getJobId() << "::x : " << x << endl;

        return job;
    }

    int execute(double* percentOfExecution, bool changeFlagTo) {
      //  cerr << getJobId() << "::execute()" << endl;

        for (int i = 0; i < 100; i++) {
 //            if (GLOBAL_EXECUTION_FLAG) {
 //                *percentOfExecution = (double)i / 100;
 //               return i;
 //           }
            // Computes the sum of magnitudes of the vector elements.
            cblas_dasum(size, x, 1);
        }
 //       GLOBAL_EXECUTION_FLAG = changeFlagTo;
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
        
        MklXpyJob* job = new MklXpyJob(scaledSize, x, y);

        cerr << job->getJobId() << "::x : " << x << endl;
        cerr << job->getJobId() << "::y : " << y << endl;

        return job;
    }

    int execute(double* percentOfExecution, bool changeFlagTo) {
        //cerr << getJobId() << "::execute()" << endl;

        for (int i = 0; i < 100; i++) {
       //     if (GLOBAL_EXECUTION_FLAG) {
       //         *percentOfExecution = (double)i / 100;
       //         return i;
       //     }
            //Computes a vector-scalar product and adds the result to a vector. y := a*x + y
            cblas_daxpy(size, 3.5, x, 1, y, 1);
        }
       // GLOBAL_EXECUTION_FLAG = changeFlagTo;
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

class MklDotJob : public Job {
private:
    double* x;
    double* y;
    volatile double dummy_res; // volatile предотвратит вырезание компилятором

public:
    int size;
    MklDotJob(int size, double* x, double* y) :
        size(size),
        x(x),
        y(y),
        dummy_res(0.0) {}

    MklDotJob(const MklDotJob &job) { x = job.x; y = job.y; dummy_res = job.dummy_res; }

    static MklDotJob* create(int size) {
        long scaledSize = size * 1000000;
        double* x = (double*)mkl_malloc(scaledSize * sizeof(double), 64);
        double* y = (double*)mkl_malloc(scaledSize * sizeof(double), 64);

        for (long i = 0; i < scaledSize; i++) {
            x[i] = 1.0;
            y[i] = 2.0;
        }

        MklDotJob* job = new MklDotJob(scaledSize, x, y);

        cerr << job->getJobId() << "::x : " << x << endl;
        cerr << job->getJobId() << "::y : " << y << endl;

        return job;
    }

    int execute(double* percentOfExecution, bool changeFlagTo) {
   //     cerr << getJobId() << "::execute()" << endl;

        for (int i = 0; i < 100; i++) {
 //            if (GLOBAL_EXECUTION_FLAG) {
 //                *percentOfExecution = (double)i / 100;
 //                return i;
 //            }
            // Считаем и пишем в volatile переменную, нагружая только каналы чтения шины
            dummy_res = cblas_ddot(size, x, 1, y, 1);
        }
  //      GLOBAL_EXECUTION_FLAG = changeFlagTo;
        *percentOfExecution = 1.0;
        return 100;
    }

    int getSize() {
        return size / 1000000;
    }

    string getType() {
        return "DOT";
    }

    Job* copy() {
         return MklDotJob::create(getSize());
    }

    ~MklDotJob() {
        mkl_free(x);
        mkl_free(y);
    }
};
