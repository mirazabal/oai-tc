#include "dummy_pcr.h"

#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

#include "../../alg_ds/meter/meter.h"
#include "../../alg_ds/alg/defer.h"
#include "../../alg_ds/ds/lock_guard/lock_guard.h"
#include "../../time/time.h"

typedef struct dummy_pcr_s{
  pcr_t base;
  mtr_t m;
  uint32_t bytes_fwd;

  pthread_mutex_t mtx;
} dummy_pcr_t;

static
void dummy_pcr_free(pcr_t* p_base)
{
  assert(p_base != NULL);
  dummy_pcr_t* p = (dummy_pcr_t*)p_base;
  mtr_free(&p->m);

  int rc = pthread_mutex_destroy(&p->mtx);
  assert(rc == 0);

  free(p);
}

static
pcr_act_e dummy_pcr_action(pcr_t* p_base, uint32_t bytes)
{
  assert(p_base != NULL);
//  dummy_pcr_t* p = (dummy_pcr_t*)p_base;
  (void)bytes;
  return PCR_PASS;
}

static
void dummy_pcr_bytes_fwd(pcr_t* p_base, uint32_t bytes)
{
  assert(p_base != NULL);
  dummy_pcr_t* p = (dummy_pcr_t*)p_base;

  // Fill statistics
  lock_guard(&p->mtx);

  p->bytes_fwd += bytes;
  int64_t const tstamp = time_now_us();
  mtr_push_back(&p->m, tstamp, bytes);
} 

static
void dummy_pcr_mod(pcr_t* p_base, tc_mod_ctrl_pcr_t const* ctrl )
{
  assert(p_base != NULL);
  assert(ctrl != NULL);
  dummy_pcr_t* p = (dummy_pcr_t*)p_base;
  (void)p;
  (void)ctrl;
  printf("No Op for dummy pcr conf \n");
} 

static
tc_pcr_t dummy_pcr_stats(pcr_t* p_base)
{
  assert(p_base != NULL);
  dummy_pcr_t* p = (dummy_pcr_t*)p_base;

  tc_pcr_t ans = {.type = TC_PCR_DUMMY };

  // Fill statistics
  {
    lock_guard(&p->mtx);

    ans.id = -1;
    ans.mtr.time_window_ms = p->m.avg_wnd.wnd_ms;
    ans.mtr.bnd_flt = mtr_bndwdth_kbps(&p->m);
  }
  return ans;
}

pcr_t* dummy_pcr_init(void)
{
  dummy_pcr_t* p = malloc(sizeof(dummy_pcr_t));
  assert(p != NULL);

  p->base.free = dummy_pcr_free;
  p->base.action = dummy_pcr_action;
  p->base.bytes_fwd = dummy_pcr_bytes_fwd;
  p->base.mod = dummy_pcr_mod;
  p->base.handle = NULL;
  p->base.stats = dummy_pcr_stats;
  p->base.type = TC_PCR_DUMMY;
 
  const float window_ms = 100.0;
  mtr_init(&p->m, window_ms);

//  p->drb_bytes = 0;
  p->bytes_fwd = 0;


  pthread_mutexattr_t *attr = NULL;
#ifdef DEBUG
  const int type = PTHREAD_MUTEX_ERRORCHECK;
  int rc_mtx = pthread_mutexattr_settype(attr, type); 
  assert(rc_mtx == 0);
#endif
  pthread_mutex_init(&p->mtx, attr);

  return &p->base;
} 

