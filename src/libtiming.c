#include "config.h"

/**
 * function: timing_res 
 * return once timing's resolution, nanosecond unit
 *
 * function: timing_err
 * return an error between two timing's, should sub this value
 *
 * function: timing
 * return a timing, mul timing_res to calc real time
 */

#if (defined TIMING_TSC) || (defined TIMING_TSCP)

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

/* We use 64bit values for the times.  */
typedef unsigned long long int hp_timing_t;

#ifdef TIMING_TSC
/** copy code from simple-pmu:cycles.h (http://halobates.de/simple-pmu) **/
static inline hp_timing_t _timing(void)
{
#ifdef __i386__
	unsigned long long s;
	asm volatile("rdtsc" : "=A" (s) :: "memory");
	return s;
#else
	unsigned low, high;
	asm volatile("rdtsc" : "=a" (low), "=d" (high) :: "memory");
	return ((unsigned long long)high << 32) | low;
#endif
}
#else /*TIMING_TSC*/
static inline hp_timing_t _timing(void)
{
#ifdef __i386__
	unsigned long long s;
	asm volatile("rdtscp" : "=A" (s) :: "ecx", "memory");
	return s;
#else
	unsigned low, high;
	asm volatile("rdtscp" : "=a" (low), "=d" (high) :: "ecx", "memory");
	return ((unsigned long long)high << 32) | low;
#endif
}
#endif /*TIMING_TSC*/

uint64_t timing_res() 
{
   const char* file = "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq";
   FILE* f = fopen(file,"r");
   if(f==NULL){
      perror("Unable Find CPU freq sys file:");
      exit(errno);
   }
   unsigned long freq = 0;
   fscanf(f, "%lu", &freq);
   if(freq==0){
      perror("Unable Read CPU freq:");
      exit(errno);
   }
   return 1*1000*1000*1000 /*nanosecond*/ / freq;
}
uint64_t timing_err()
{
   hp_timing_t a = 0,b = 0;
   a=_timing();
   b=_timing();
   return b-a;
}
uint64_t timing()
{
   return _timing();
}


#endif

#ifdef TIMING_TSCP
#endif

#ifdef TIMING_CLOCK_GETTIME
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define CLK_ID CLOCK_PROCESS_CPUTIME_ID

uint64_t timing_res() 
{
   return 1;
}
uint64_t timing_err()
{
   struct timespec a = {0}, b = {0};
   long sum = 0;
   for(int i=0;i<100;++i){
      clock_gettime(CLK_ID, &a);
      clock_gettime(CLK_ID, &b);
      sum += (b.tv_sec - a.tv_sec)*10E9;
      sum += b.tv_nsec - a.tv_nsec;
   }
   return sum /=100;
}
uint64_t timing()
{
   struct timespec t = {0};
   clock_gettime(CLK_ID, &t);
   return t.tv_sec*10E9+t.tv_nsec;
}
#endif

