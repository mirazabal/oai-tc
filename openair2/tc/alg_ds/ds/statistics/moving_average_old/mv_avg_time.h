#ifndef MIR_MOVING_AVERAGE_TIME_WINDOW
#define MIR_MOVING_AVERAGE_TIME_WINDOW 

#include "../../seq_container/seq_generic.h"

typedef struct{
  seq_ring_t tstamps; // int64_t
  seq_ring_t val; // uint32_t
  double avg;
  double wnd_ms;
} mv_avg_wnd_t;

void mv_avg_wnd_init(mv_avg_wnd_t*, float wnd_ms);

void mv_avg_wnd_free( mv_avg_wnd_t* );

void mv_avg_wnd_push_back(mv_avg_wnd_t*, int64_t tstamp, uint32_t val);

double mv_avg_wnd_val(mv_avg_wnd_t*);

#endif

