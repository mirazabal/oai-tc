/*
MIT License

Copyright (c) 2021 Mikel Irazabal

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#include "mv_avg_elm.h"
#include "mv_avg_time.h"
#include <time.h>
#include <stdio.h>
#include <unistd.h>

#include "../../../alg/defer.h"

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


