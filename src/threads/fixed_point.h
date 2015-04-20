#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

/*note that: n is not dealt with as a fixed_point(17.14), it is an integer*/

/*this class is has no struct and is thus considered a library of functions*/

int fp_int_to_fp(int n);
int fp_to_int_zero(int x);
int fp_to_int_nearest(int x);
int fp_add(int x, int y);
int fp_add_mix(int x, int n);
int fp_sub(int x, int y);
int fp_sub_mix(int x, int n);
int fp_mult(int x, int y);
int fp_mult_mix(int x, int y);
int fp_div(int x, int y);
int fp_div_mix(int x, int n);

#endif
