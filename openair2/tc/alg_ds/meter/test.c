
#include "meter.h"

#include <time.h>
#include <stdio.h>
#include <unistd.h>

/*
static
int64_t time_now_us()
{
  struct timespec tms;

 // The C11 way 
 // if (! timespec_get(&tms, TIME_UTC))  

  // POSIX.1-2008 way 
  if (clock_gettime(CLOCK_REALTIME,&tms)) {
    return -1;
  }
  // seconds, multiplied with 1 million 
  int64_t micros = tms.tv_sec * 1000000;
  // Add full microseconds 
  micros += tms.tv_nsec/1000;
  // round up if necessary 
  if (tms.tv_nsec % 1000 >= 500) {
    ++micros;
  }
  return micros;
}
*/


int main()
{
  mtr_t m = {0};
  float const window_ms = 100;
  mtr_init(&m, window_ms);

  int64_t tstamp = 0;
  const uint32_t val = 1500;


  for(int i = 0; i < 1024; ++i){
    uint32_t rand_it = abs(rand()%32); 
    for(int j = 0; j < rand_it; ++j){
      mtr_push_back(&m, tstamp, val);
      uint32_t s = abs(rand()%1000);
      usleep(s);
    }

    printf("Bandwidth = %f \n", mtr_bndwdth_kbps(&m) );
  }

  mtr_free(&m);

  return EXIT_SUCCESS;
}
