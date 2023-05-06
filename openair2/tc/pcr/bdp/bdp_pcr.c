#include "bdp_pcr.h"
#include "../../alg_ds/alg/alg.h"
#include "../../alg_ds/alg/rotate.h"
#include "../../alg_ds/meter/meter.h"
#include "../../time/time.h"
#include "../../alg_ds/alg/defer.h"
#include "../../alg_ds/ds/lock_guard/lock_guard.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MTU_SIZE 1514

typedef struct bdp_pcr_s
{
  pcr_t base;
  mtr_t m;

  float bndwdth[4]; // 3 + 1 for last
  int64_t last_time;
  int64_t last_slack_time;
  uint32_t tx_bytes; 
  uint32_t drb_bytes;
  int64_t drb_bytes_tstamp; 
  uint32_t last_drb_bytes;
  pthread_mutex_t mtx;
  pthread_mutex_t mtx_stats;

  bool update_flag;
} bdp_pcr_t;


static
float phy_tx_bytes(int32_t tx_bytes, int32_t drb_bytes, int32_t last_drb_bytes)
{
  int64_t phy_tx_bytes = tx_bytes - (drb_bytes - last_drb_bytes);
  if (phy_tx_bytes < 0)
    phy_tx_bytes = 0;

  assert(phy_tx_bytes < 80000); // multithreaded enviroment...
  return phy_tx_bytes;
}
 
// Bandwidth Exponential Weight Moving Average
static
float ewma_bndwdth(bdp_pcr_t* p, float bndwdth) 
{
  const float alfa = 0.7;
  return alfa * bndwdth + (1 - alfa) * p->bndwdth[0] +
      (1 - alfa) * (1 - alfa) * p->bndwdth[1] +
      (1 - alfa) * (1 - alfa) * (1 - alfa) * p->bndwdth[2];
}

static
void update_bdp_vals(bdp_pcr_t* p)
{
  const float phy_tx_bytes_v = phy_tx_bytes(p->tx_bytes, p->drb_bytes, p->last_drb_bytes);
  const int64_t now = time_now_us();
  const int64_t remainder = (now - p->last_time) % 1000 > 500 ? 1 : 0;
  const int64_t slack_time_i = (((now - p->last_time) / 1000) + remainder) *
                               1000; // now - pacer->last_time;
  const float slack_time = slack_time_i != 0 ? (float)slack_time_i : 1000.0;
  p->last_slack_time = slack_time;
  const float last_bndwdth = phy_tx_bytes_v / slack_time;
  const float bndwdth = ewma_bndwdth(p, last_bndwdth);
  assert(bndwdth < 400);

  p->last_time = now;
  p->last_drb_bytes = p->drb_bytes;
  p->tx_bytes = 0;

  rotate(&p->bndwdth[0], &p->bndwdth[1], &p->bndwdth[3], sizeof(float));
  p->bndwdth[0] = bndwdth;
}

static
int64_t th_tx_bytes(bdp_pcr_t *p, int64_t last_time)
{
  const int64_t now = time_now_us(); 
  int64_t elapsed_time = now - last_time; 
  if (elapsed_time > 1200) {
    printf("WARNING: elapsed time above 1000 ms, elapsed_time = %ld \n",
           elapsed_time);
    elapsed_time = 1000;
  }
  const float perc_slack = (float)elapsed_time / 1000; 
  const float mult_factor = perc_slack < 0.5 ? 1.2 : 1.5;

  int64_t theoretical_tx =
      p->bndwdth[0] > 0.1
          ? mult_factor * elapsed_time * p->bndwdth[0] + 1400*2
          : p->drb_bytes < 15000 && elapsed_time > 0.5 && p->tx_bytes == 0 ? MTU_SIZE : 0;

  assert(p->bndwdth[0] < 400);
  if(theoretical_tx  < 15000){
    theoretical_tx = 15000; 
  }
  assert(theoretical_tx < 80000);
  //printf("Theoretical tx = %ld \n", theoretical_tx );
  return theoretical_tx;
}

static
void bdp_pcr_free(pcr_t* p_base)
{
  assert(p_base != NULL);
  bdp_pcr_t* p = (bdp_pcr_t*)p_base; 
  int rc = pthread_mutex_destroy(&p->mtx);
  assert(rc == 0);

  mtr_free(&p->m);

  rc = pthread_mutex_destroy(&p->mtx_stats);
  assert(rc == 0);

  free(p);
}

static
pcr_act_e bdp_pcr_action(pcr_t* p_base, uint32_t bytes)
{
  assert(p_base != NULL);
  bdp_pcr_t* p = (bdp_pcr_t*)p_base; 
  assert(bytes < MTU_SIZE + 1 && "IP packets are not bigger than a MTU!");
 

  lock_guard(&p->mtx);

  if (p->update_flag == true) {
    //puts("Updating the bdp");
    p->update_flag = false;
    update_bdp_vals(p); 
  }
//  const uint32_t max_bytes_per_tti = 2300; // Value for 25 PRBs and 28 MCS
  const uint32_t max_bytes_per_tti = 40000; // Value for 25 PRBs and 28 MCS
  if (p->tx_bytes + p->drb_bytes > max_bytes_per_tti) {
    puts("First wait" );
    printf("p->tx_bytes %d p->drb_bytes %d > max_bytes_per_tti %d \n", p->tx_bytes, p->drb_bytes, max_bytes_per_tti);
    return PCR_WAIT; 
  }

  const int32_t extra_packet = p->tx_bytes != 0 && p->drb_bytes != 0
                                   ? (int32_t)(bytes) / 5
                                   : (int32_t)(bytes) / 3;

  const int64_t th_tx_bytes_v = th_tx_bytes(p, p->last_time);
  if (p->tx_bytes + p->drb_bytes + extra_packet > th_tx_bytes_v){ 
    puts("Second wait" );
    printf("p->tx_bytes %d p->drb_bytes %d extra_packet %d th_tx_bytes_v %ld \n ", p->tx_bytes, p->drb_bytes, extra_packet,  th_tx_bytes_v   );
    return PCR_WAIT;
  } 
  return PCR_PASS;
}

static
void bdp_pcr_bytes_fwd(pcr_t* p_base, uint32_t bytes)
{
  assert(p_base != NULL);
  bdp_pcr_t* p = (bdp_pcr_t*)p_base; 

  // Fill statistics
  lock_guard(&p->mtx_stats);

  p->tx_bytes += bytes;
  int64_t const tstamp = 0;
  mtr_push_back(&p->m, tstamp, bytes);
}

static
void bdp_pcr_mod(pcr_t* p_base, tc_mod_ctrl_pcr_t const* mod)
{
  assert(p_base != NULL);
  bdp_pcr_t* p = (bdp_pcr_t*)p_base; 
  assert(mod != NULL);

  lock_guard(&p->mtx);

  p->update_flag = true;
  p->drb_bytes = mod-> bdp.drb_sz;
  //if( mod->bdp.tstamp - p->drb_bytes_tstamp > 1500){
  printf("Time difference BDP in us = %ld RLC DRB size = %d \n",  mod->bdp.tstamp - p->drb_bytes_tstamp, mod->bdp.drb_sz );
  //}
  p->drb_bytes_tstamp = mod->bdp.tstamp;
}

static
tc_pcr_t bdp_pcr_stats(pcr_t* p_base)
{
  assert(p_base != NULL);
  bdp_pcr_t* p = (bdp_pcr_t*)p_base; 

  tc_pcr_t ans = {.type = TC_PCR_5G_BDP };

  ans.id = -1;

  // Fill statistics
  { 
    lock_guard(&p->mtx_stats);
    //int rc = pthread_mutex_lock(&p->mtx_stats);
    //assert(rc == 0);
    //defer( { rc = pthread_mutex_unlock(&p->mtx_stats); assert(rc == 0); } );

    ans.mtr.time_window_ms = p->m.avg_wnd.wnd_ms;
    //window_ms;
    ans.mtr.bnd_flt = mtr_bndwdth_kbps(&p->m);
  }
  return ans;
}

pcr_t* bdp_pcr_init(void)
{
  bdp_pcr_t* p = malloc(sizeof(bdp_pcr_t));
  assert(p != NULL);
  p->base.free = bdp_pcr_free; 
  p->base.action = bdp_pcr_action;
  p->base.bytes_fwd = bdp_pcr_bytes_fwd;
  p->base.mod = bdp_pcr_mod;
  p->base.handle = NULL;
  p->base.stats = bdp_pcr_stats;
  p->base.type = TC_PCR_5G_BDP;

  memset(&p->bndwdth, 0, sizeof(float)*4); 
 
  p->bndwdth[0] = 40.0;

  p->last_time = time_now_us();
  p->last_slack_time = 1000;

  p->tx_bytes = 0;
  p->drb_bytes = 0;
  p->drb_bytes_tstamp = 0;
  p->last_drb_bytes = 0;

  pthread_mutexattr_t *attr = NULL;
#ifdef DEBUG
  const int type = PTHREAD_MUTEX_ERRORCHECK;
  int rc_mtx = pthread_mutexattr_settype(attr, type); 
  assert(rc_mtx == 0);
#endif

  int rc = pthread_mutex_init(&p->mtx, attr);
  assert(rc == 0 && "Error creating the bdp pacer mutex");

  rc = pthread_mutex_init(&p->mtx_stats, attr);
  assert(rc == 0);

  p->update_flag = false; 

  float const time_window_ms = 100.0;
  mtr_init(&p->m, time_window_ms );

  printf("BDP pacer init \n");

  return &(p->base);
} 

