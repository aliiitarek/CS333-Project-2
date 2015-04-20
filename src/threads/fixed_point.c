#include <stdint.h>
#include <stddef.h>
#include <debug.h>
#include "threads/fixed_point.h"

#define f (1<<14)

int
fp_int_to_fp(int n)
{
  return n*f;
}

int
fp_to_int_zero(int x)
{
  return x/f;
}

int
fp_to_int_nearest(int x)
{
  if (x >= 0)
      return (x+f/2)/f;
  else
      return (x-f/2)/f;
}

int
fp_add(int x, int y)
{
  return x+y;
}

int
fp_add_mix(int x, int n)
{
  return x+fp_int_to_fp(n);
}

int
fp_sub(int x, int y)
{
  return x-y;
}

int
fp_sub_mix(int x, int n)
{
  return x-fp_int_to_fp(n);
}

int
fp_mult(int x, int y)
{
  return (((int64_t) x)*y)/f;
}

int
fp_mult_mix(int x, int n)
{
  return x*n;
}

int
fp_div(int x, int y)
{
  return ((int64_t) x)*f/y;
}

int
fp_div_mix(int x, int n)
{
  return x/n;
}
