#ifndef POLICER_SHAPER_PAIR_H
#define POLICER_SHAPER_PAIR_H 

#include "plc/plc.h"
#include "shp/shp.h"
#include "alg_ds/ds/assoc_container/assoc_generic.h"

typedef struct {
  plc_t* plc;
  shp_t* shp;
} plc_shp_pair_t;

plc_shp_pair_t* init_pcr_shp( plc_t** p, shp_t** s);

void free_plc_shp(plc_shp_pair_t* pair);

plc_shp_pair_t* find_plc_shp(assoc_ht_open_t* htab, queue_t** q);

#endif


