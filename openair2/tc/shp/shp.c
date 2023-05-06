
#include "../alg_ds/alg/alg.h"
#include "../alg_ds/alg/accumulate.h"
#include "../time/time.h"
#include "shp.h"

#include <assert.h>
#include <stdio.h>


shp_t* shp_init(uint32_t time_window_us, uint32_t max_rate_kbps)
{
  shp_t* s = malloc(sizeof(shp_t));
  assert(s != NULL);

  mtr_init(&s->m, time_window_us/1000);

  s->max_rate_kbps = max_rate_kbps;
  s->active = true;
  return s;
}

void shp_free(shp_t* s)
{
  assert(s != NULL);

  mtr_free(&s->m);

  free(s);
}

// Inform the shaper that pkts have been dequeued
void shp_bytes_fwd(shp_t* s, uint32_t  bytes)
{
  assert(s != NULL);
  assert(bytes != 0);

  int64_t tstamp = 0; // the meter will call the time_now function
  uint32_t bits = 8*bytes;
  assert(bits > bytes && "Overflow detected");
  mtr_push_back(&s->m, tstamp, bits);
}


shp_act_e shp_action(shp_t* s, uint32_t bytes)
{
  assert(s != NULL);
  assert(bytes != 0);

  float rate_kbps = mtr_bndwdth_kbps(&s->m);

//  printf("Shaper rate = %f\n", rate_kbps);

  //if(rate_kbps > s->max_rate_kbps * 1000)
  if(rate_kbps > s->max_rate_kbps)
    return SHP_WAIT;

  return SHP_PASS;
}


tc_shp_t shp_stat(shp_t* s)
{
  assert(s != NULL);

  tc_shp_t ans = {.active = s->active };

  ans.max_rate_kbps = s->max_rate_kbps;

  if(ans.active == true){
    ans.mtr.time_window_ms  = s->m.avg_wnd.wnd_ms;
    ans.mtr.bnd_flt = s->m.avg_wnd.avg;
  }
  
  return ans;
}

void shp_mod(shp_t* s, tc_mod_ctrl_shp_t const* mod)
{
  assert(s != NULL);
  assert(mod != NULL);

  s->active = mod->active;
  s->m.avg_wnd.wnd_ms = mod->time_window_ms;
  s->max_rate_kbps = mod->max_rate_kbps; 

  printf("Shaper modified successfully \n" );
}


