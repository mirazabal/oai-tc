#include "mv_avg_elm.h"
#include "mv_avg_time.h"
#include <time.h>
#include <stdio.h>
#include <unistd.h>

#include "../alg_ds/alg/defer.h"

static
void test_mv_avg_elm()
{
  mv_avg_elm_t m = {0};

  mv_avg_elm_init(&m, 10); 
  defer({  mv_avg_elm_free(&m);  } );

  for(int i = 0; i < 1024; ++i){
    uint32_t rand_it = abs(rand()%32); 
    for(int j = 0; j < rand_it; ++j){

      const uint32_t val = rand()%1500 +1;
      mv_avg_elm_push_back(&m, val);
      uint32_t s = abs(rand()%1000);
      usleep(s);
    }
    printf("Average elm = %f \n", mv_avg_elm_val(&m) );
  }

}


static
void test_mv_avg_wnd()
{
  mv_avg_wnd_t m = {0};

  double tm_wnd_ms = 100.0;
  mv_avg_wnd_init(&m, tm_wnd_ms); 
  defer({  mv_avg_wnd_free(&m);  } );

  int64_t const tstamp = 0;
  for(int i = 0; i < 1024; ++i){
    uint32_t rand_it = abs(rand()%32); 
    for(int j = 0; j < rand_it; ++j){

      const uint32_t val = rand()%1500 +1;
      mv_avg_wnd_push_back(&m, tstamp, val);
      uint32_t s = abs(rand()%1000);
      usleep(s);
    }
    printf("Average wnd = %f \n", mv_avg_wnd_val(&m) );
  }

}

int main()
{

  test_mv_avg_elm();
  test_mv_avg_wnd();

  return EXIT_SUCCESS;
}


