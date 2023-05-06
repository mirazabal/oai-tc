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


#include "mv_avg_time.h"

#include <assert.h>

#include "../../../alg/accumulate.h"
#include "../../../alg/defer.h"
#include "../../../alg/lower_bound.h"

#include <stdbool.h>
#include <stdint.h>

#include <time.h>

static
int64_t time_now_us()
{
  struct timespec tms;

  /* The C11 way */
  /* if (! timespec_get(&tms, TIME_UTC))  */

  /* POSIX.1-2008 way */
  if (clock_gettime(CLOCK_REALTIME,&tms)) {
    return -1;
  }
  /* seconds, multiplied with 1 million */
  int64_t micros = tms.tv_sec * 1000000;
  /* Add full microseconds */
  micros += tms.tv_nsec/1000;
  /* round up if necessary */
  if (tms.tv_nsec % 1000 >= 500) {
    ++micros;
  }
  return micros;
}

static inline
bool mv_avg_wnd_invariants(mv_avg_wnd_t* m)
{
  assert(m != NULL);
  size_t const sz_t = seq_size(&m->tstamps); 
  size_t const sz_v = seq_size(&m->val);
  assert(sz_t == sz_v);
  assert(!(m->avg < 0.0));
  assert(m->wnd_ms > 0.0);

  return true;
}

static inline
bool tstamp_invariant(mv_avg_wnd_t* m, int64_t tstamp)
{
  assert(m != NULL);
  assert(tstamp > 0);

  const size_t sz = seq_size(&m->tstamps);
  void* l = seq_at(&m->tstamps, sz -1);

  assert(tstamp >= *(int64_t*)l && "Time is a monotonically increasing function");

  return true;
}

void mv_avg_wnd_init(mv_avg_wnd_t* m, float wnd_ms)
{
  assert(m != NULL);
  assert(wnd_ms > 0.0);
  defer({ assert(mv_avg_wnd_invariants(m)); } );

  seq_init(&m->tstamps, sizeof(int64_t));
  seq_init(&m->val, sizeof(uint32_t));

  m->avg = 0.0;
  m->wnd_ms = wnd_ms;
}

void mv_avg_wnd_free( mv_avg_wnd_t* m)
{
  assert(m != NULL);
  assert(mv_avg_wnd_invariants(m));

  void* value_semantic = NULL;
  seq_free(&m->tstamps, value_semantic);
  seq_free(&m->val, value_semantic);
}

void mv_avg_wnd_push_back(mv_avg_wnd_t* m, int64_t tstamp, uint32_t val)
{
  assert(m != NULL);
  assert(!(tstamp < 0));
  assert(val != 0);
  assert(mv_avg_wnd_invariants(m));
  defer({ assert(mv_avg_wnd_invariants(m)); } );

  if(tstamp == 0)
    tstamp = time_now_us();

  assert(tstamp_invariant(m,tstamp) == true);
 
  size_t elm = seq_size(&m->tstamps);
  if(elm >= 32768 ){
    // Call wnd_val to liberate memory
    mv_avg_wnd_val(m);
    elm = seq_size(&m->tstamps);
    assert(elm < 32768 && "Memory is increasing dangerously, maybe you should liberate using  mv_avg_wnd_val?");
  }
  assert(elm < 32768 && "Memory is increasing dangerously, maybe you should liberate using  mv_avg_wnd_val?");

  seq_push_back(&m->tstamps, (uint8_t*)&tstamp, sizeof(tstamp));
  seq_push_back(&m->val, (uint8_t*)&val, sizeof(val));

  m->avg = (m->avg*elm + val)/(elm+1); // + val/(elm+1);  

}

static inline
bool cmp_int64(void* val, void* it)
{
  int64_t* a = (int64_t*)val;
  int64_t* b = (int64_t*)it;

  return *a > *b;
}

double mv_avg_wnd_val(mv_avg_wnd_t* m)
{
  assert(m != NULL);
  assert(mv_avg_wnd_invariants(m) == true);
  defer({ assert(mv_avg_wnd_invariants(m)); } );

  size_t const elm = seq_size(&m->tstamps);
  if(elm == 0) 
    return 0.0;

  int64_t const now_us = time_now_us();

  void* f_ts = seq_front(&m->tstamps);
  void* l_ts = seq_end(&m->tstamps); 

  int64_t const limit_us = now_us - m->wnd_ms*1000; 
  void* it = lower_bound_ring(&m->tstamps, f_ts, l_ts, (void*)&limit_us, cmp_int64);

  uint32_t n = 0;
  if(it != f_ts){
    n = seq_distance(&m->tstamps, f_ts, it);
    // Check that lower_bound worked correctly
    int64_t const last_tstamp = *(int64_t*)seq_at(&m->tstamps, n -1);
    assert(last_tstamp < limit_us);
  }
  assert(n <= elm);

  seq_erase(&m->tstamps, f_ts, it);

  void* f_val = seq_front(&m->val);
  void* l_val = seq_at(&m->val, n); 

  uint64_t const acc = accumulate_ring(&m->val,f_val, l_val);

  seq_erase(&m->val, f_val, l_val);

  if(n < elm)
    m->avg = (m->avg*elm - acc)/(elm-n);
  else{ // n == elm all the data deleted
    assert(seq_size(&m->val) == 0);

    m->avg = 0.0;
  }

  assert(m->avg > -1.0);
  return m->avg;

//  size_t const sz = seq_size(&m->tstamps); 
//  return (m->avg*sz / m->window_ms) ;
}

