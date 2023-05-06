#include "plc.h"
#include "../time/time.h"
#include "../alg_ds/alg/alg.h" 
#include "../alg_ds/alg/find.h" 

#include "../alg_ds/alg/accumulate.h" 
#include "../alg_ds/ds/seq_container/seq_generic.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MTU 1514

typedef struct pair_tstamp_pkt_e 
{
  uint32_t pkt_bytes;
  int64_t tstamp;
} pair_tstamp_pkt_t;


plc_t* plc_init(uint32_t drop_rate_kbps, uint32_t dev_rate_kbps, uint32_t time_window_us, queue_t* dst_q, queue_t* dev_q)  
{
  plc_t* plc = calloc(1,sizeof(plc_t));
  assert(plc != NULL);

  mtr_init(&plc->m, time_window_us / 1000);

  plc->dropped_pkts = 0;
  plc->active = 1;

  plc->drop_rate_kbps = drop_rate_kbps;
  plc->dev_rate_kbps = dev_rate_kbps;

  plc->dst_q = dst_q;
  plc->dev_q = dev_q;

  return plc;
} 

void plc_free(plc_t* plc)
{
  assert(plc != NULL);
  mtr_free(&plc->m);
  free(plc);
}

void plc_bytes_fwd(plc_t* plc, uint32_t bytes)
{
  assert(plc != NULL);
  assert(bytes != 0);

  int64_t const tstamp = 0; // The tstamp will be taken from the meter
  mtr_push_back(&plc->m, tstamp, bytes);
}

plc_act_e plc_action(plc_t* plc, uint32_t bytes)
{
  assert(plc != NULL);
  assert(bytes != 0);

  float const rate_kbps = mtr_bndwdth_kbps(&plc->m);
  if(rate_kbps > plc->drop_rate_kbps){
    printf("Policer rate = %f kbps\n", rate_kbps);
    printf("Policer Dropping packet \n");
    ++plc->dropped_pkts;
    return PLC_DROP;
  }

  if(rate_kbps > plc->dev_rate_kbps){
    return PLC_DEVIATE;
  }

  return PLC_PASS;
}

void plc_set_dev_q(plc_t* plc, queue_t* dev_q)
{
  assert(plc != NULL);
  assert(dev_q != NULL);
  plc->dev_q = dev_q;
}

tc_plc_t plc_stat(plc_t* plc)
{
  assert(plc != NULL);

  tc_plc_t ans = {0}; 

  ans.active = plc->active;
  ans.dev_id = plc->dev_q->id; 
  ans.dst_id = plc->dst_q->id;
  //
  ans.drp.dropped_pkts = plc->dropped_pkts;

  ans.mtr.time_window_ms = plc->m.avg_wnd.wnd_ms;
  ans.mtr.bnd_flt = plc->m.avg_wnd.avg;

  ans.mrk.marked_pkts = 0; // ToDO?? 

  return ans;
}

void plc_mod(plc_t* p, queue_t** dev_q, tc_mod_ctrl_plc_t const* ctrl)
{
  assert(p != NULL);
  assert(ctrl != NULL);
  
  p->active = ctrl->active;
  p->dev_rate_kbps = ctrl->dev_rate_kbps;
  p->drop_rate_kbps = ctrl->drop_rate_kbps;
  p->dev_q = *dev_q;

  printf("Policer modified successfully \n");
}

