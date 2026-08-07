#ifndef ROBU_LIBC_MATH_H
#define ROBU_LIBC_MATH_H
double sqrt(double x);
double pow(double x, double y);
double floor(double x);
double ceil(double x);
double fabs(double x);
double log(double x);
double log10(double x);
double fmod(double x, double y);
double round(double x);
double sin(double x);
double cos(double x);
double tan(double x);
double atan2(double y, double x);
#define HUGE_VAL (__builtin_huge_val())
#define NAN (__builtin_nanf(""))
#define INFINITY (__builtin_inff())
#endif
