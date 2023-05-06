#ifndef MIR_METER_H
#define MIR_METER_H

// #include "../ds/seq_container/seq_generic.h"

#include "../ds/statistics/moving_average/mv_avg_time.h"
#include <stdint.h>

typedef struct{
/*
  seq_ring_t tstamps; // int64_t
  seq_ring_t val; // uint32_t
  float avg;
  int64_t window_ms;
  */
  mv_avg_wnd_t avg_wnd;

} mtr_t;

void mtr_init(mtr_t* m, float window_ms);

void mtr_free(mtr_t* m);

void mtr_push_back(mtr_t* m, int64_t tstamp, uint32_t val);

float mtr_bndwdth_kbps(mtr_t* m);

#endif

