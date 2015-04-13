#include "benchmark.h"

struct timeval tv_sub(struct timeval tv_end, struct timeval tv_start)
{
  struct timeval tv_res;

  if (tv_end.tv_usec < tv_start.tv_usec)
  {
    time_t sec = (tv_start.tv_usec - tv_end.tv_usec) / 1000000 + 1;
    tv_start.tv_usec -= 1000000 * sec;
    tv_start.tv_sec += sec;
  }

  if (tv_end.tv_usec - tv_start.tv_usec > 1000000)
  {
    time_t sec = (tv_end.tv_usec - tv_start.tv_usec) / 1000000;
    tv_start.tv_usec += 1000000 * sec;
    tv_start.tv_usec -= sec;
  }

  tv_res.tv_sec = tv_end.tv_sec - tv_start.tv_sec;
  tv_res.tv_usec = tv_end.tv_usec - tv_start.tv_usec;

  return tv_res;
}
