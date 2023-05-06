#ifndef MIR_MOVING_AVERAGE_TIME_ELEMENTS
#define MIR_MOVING_AVERAGE_TIME_ELEMENTS 

#include "../../seq_container/seq_generic.h"

#include <stdint.h>

typedef struct{
  seq_ring_t val; // uint32_t
  uint32_t elm;
  double avg;
} mv_avg_elm_t;

void mv_avg_elm_init(mv_avg_elm_t*, uint32_t elm);

void mv_avg_elm_free(mv_avg_elm_t*);

void mv_avg_elm_push_back(mv_avg_elm_t*, uint32_t val);

double mv_avg_elm_val(mv_avg_elm_t*);

#endif

