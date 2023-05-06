#include "meter.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>

#include "../alg/defer.h"
#include "../alg/accumulate.h"
#include "../alg/lower_bound.h"

void mtr_init(mtr_t* m, float window_ms)
{
  assert(m != NULL);
  assert(window_ms > 0);

  mv_avg_wnd_init(&m->avg_wnd, window_ms);

}

void mtr_free(mtr_t* m)
{
  assert(m != NULL);

  mv_avg_wnd_free(&m->avg_wnd);
}

void mtr_push_back(mtr_t* m, int64_t tstamp, uint32_t val)
{
  assert(m != NULL);
  assert(!(tstamp < 0));
  assert(val != 0);

  mv_avg_wnd_push_back(&m->avg_wnd, tstamp, val);
}
float mtr_bndwdth_kbps(mtr_t* m)
{
  assert(m != NULL);

  double val = mv_avg_wnd_val(&m->avg_wnd);
  size_t const sz = seq_size(&m->avg_wnd.tstamps); 
  return (val*sz / m->avg_wnd.wnd_ms);
}

