#ifndef __BENCHMARK_H__
#define __BENCHMARK_H__

#include <stdio.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Execute the given statement and store a textual description of the number of elapsed
   seconds in the given string.
 */
#define TIME(stmt, str)                        \
  {                                            \
    struct timeval __tv, __tv_start, __tv_end; \
    long __sec;                                \
    int __msec, __csec;                        \
    gettimeofday(&__tv_start, NULL);           \
    stmt;                                      \
    gettimeofday(&__tv_end, NULL);             \
    __tv = tv_sub(__tv_end, __tv_start);       \
    __sec = (long)__tv.tv_sec;                 \
    __msec = (int)(__tv.tv_usec / 1000);       \
   sprintf(str, "%ld.%03d", __sec, __msec);    \
  }

struct timeval tv_sub(struct timeval tv_end, struct timeval tv_start);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __BENCHMARK_H__ */
