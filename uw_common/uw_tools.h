#ifndef ns_UWTOOLS_H
#define ns_UWTOOLS_H

#include <stdio.h>
#include <gsl/gsl_cdf.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <time.h>

#define MAX_NORMAL 100

class UWTools{
  public:
    UWTools();
    ~UWTools();
    void genNormalDis();
    bool ifSend(double);
  private:
    double T[MAX_NORMAL];
    double sigma;
    double P,Q;
};
#endif