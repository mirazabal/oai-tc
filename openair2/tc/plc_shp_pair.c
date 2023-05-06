#include "plc_shp_pair.h"

#include <assert.h>

plc_shp_pair_t* init_pcr_shp( plc_t** p, shp_t** s)
{
  plc_shp_pair_t* pair = malloc(sizeof(plc_shp_pair_t));
  assert(pair != NULL);
  pair->plc = *p;
  pair->shp = *s;
  return pair;
}

void free_plc_shp(plc_shp_pair_t* pair)
{
  assert(pair != NULL);
  plc_free(pair->plc);
  shp_free(pair->shp);
  free(pair);
}

plc_shp_pair_t* find_plc_shp(assoc_ht_open_t* htab, queue_t** q)
{
  uintptr_t key = (uintptr_t)(*q);
  plc_shp_pair_t* ret = assoc_value(htab, &key);
  assert(ret != NULL && "All the queues have associated a plc and shp" );
  return ret;
}

