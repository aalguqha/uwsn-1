#include "uw_tools.h"


UWTools::UWTools(){
  sigma = 1.0;
  P = gsl_cdf_ugaussian_P (sigma);
  Q = gsl_cdf_ugaussian_Q (sigma);
  genNormalDis();
}

UWTools::~UWTools(){
  delete [] T;
}

void UWTools::genNormalDis(){
  int i;
  gsl_rng *r = gsl_rng_alloc(gsl_rng_mt19937);
  for(i=0;i<MAX_NORMAL;i++){
    T[i] = gsl_ran_gaussian(r,sigma);
  }
}

bool UWTools::ifSend(double now){
  int t = now*1000000;
  srand(t);
  double f = rand()/ (double)(RAND_MAX);
  int j = f*100;
  double gauss = 0;
  gauss = T[j];
  printf("dis_possion the rand's %d and gauss is %f at %f %d\n",j,gauss,now,t);
  return gauss <= sigma;
}
