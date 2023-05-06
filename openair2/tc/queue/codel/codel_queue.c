#include "codel_queue.h"
#include "../../time/time.h"
#include "../../alg_ds/ds/seq_container/seq_generic.h"
#include "../../alg_ds/alg/defer.h"
#include "../../alg_ds/ds/lock_guard/lock_guard.h"
#include "../../alg_ds/ds/statistics/moving_average/mv_avg_time.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>

//static const 
//int64_t TARGET = 5000; //  5 ms TARGET queue delay 

//static const
//int64_t INTERVAL = 100000; // 100 ms sliding-minimum window

static const
int64_t MAX_CODEL_QUEUE_CAP = 8192;

typedef struct{
  uint8_t* data;
  size_t bytes;
  int64_t tstamp;
} codel_pkt_t;


typedef struct{
  queue_t base;
  seq_ring_t impl;

  void (*del)(void*);

  size_t bytes;
//  size_t sz; // For debugging purposes
  // CoDel specifics 
  int64_t first_above_time_; // Time to declare sojourn time above TARGET 
  int64_t drop_next_;        // Time to drop next packet  
  uint32_t count_ ;          // Packets dropped in drop state
  uint32_t lastcount_;      // Count from previous iteration 
  uint32_t maxpacket_;      // MTU max packet size

  uint32_t bytes_fwd;
  uint32_t pkts;
  uint32_t pkts_fwd;
  int64_t last_sojourn_time;
  uint32_t dropped_pkts;

  uint32_t interval_us;
  uint32_t target_us;

  mv_avg_wnd_t avg;

  pthread_mutex_t mtx;

//  uint32_t id; // codel queue_id
  bool dropping_;     // Set to true if in drop state
} codel_queue_t;

typedef struct {
  codel_pkt_t* p; 
  bool ok_to_drop; 
} dodequeue_result_t;

static
size_t codel_size(queue_t* q_base)
{
  assert(q_base != NULL);
  codel_queue_t* q = (codel_queue_t*)(q_base);
  return seq_size(&q->impl);
}

static
size_t codel_bytes(queue_t* q_base)
{
  assert(q_base != NULL);
  codel_queue_t* q = (codel_queue_t*)(q_base);
  return q->bytes; 
}

static
dodequeue_result_t dodequeue(codel_queue_t *q, int64_t now) {  
  assert(now > -1);
  assert(q != NULL);
  codel_pkt_t* p = seq_front(&q->impl);
  assert(p->tstamp != 0);

  dodequeue_result_t r = {.p = p, .ok_to_drop = false }; 
  const size_t sz = codel_size((queue_t*)q);
  if (sz == 0){
    // queue is empty - we can’t be above TARGET 
    r.p = NULL;
    q->first_above_time_ = 0;  
    return r;  
  } 
  assert(r.p->data != NULL);
  // To span a large range of bandwidths, CoDel runs two 
  // different AQMs in parallel.  One is based on sojourn time 
  // and takes effect when the time to send an MTU-sized 
  // packet is less than TARGET.  The 1st term of the "if" 
  // below does this.  The other is based on backlog and takes
  // effect when the time to send an MTU-sized packet is >= 
  // TARGET.  The goal here is to keep the output link 
  // utilization high by never allowing the queue to get
  // smaller than the amount that arrives in a typical
  // interarrival time (MTU-sized packets arriving spaced  
  // by the amount of time it takes to send such a packet on  
  // the bottleneck).  The 2nd term of the "if" does this.
  int64_t const sojourn_time = now - r.p->tstamp;
  printf("CoDel sojourn_time = %ld tstamp = %ld \n", sojourn_time, time_now_us() );
  if (sojourn_time < q->target_us || codel_bytes((queue_t*)q) <= q->maxpacket_) {   
    // went below - stay below for at least INTERVAL 
    q->first_above_time_ = 0; 
  } else {  
    if (q->first_above_time_ == 0) {  
      // just went above from below. if still above at  
      // first_above_time, will say it’s ok to drop.
      q->first_above_time_ = now + q->interval_us;  
    } else if (now >= q->first_above_time_) {  
      r.ok_to_drop = true;
      assert(r.p != NULL);
    }   
  } 
  return r; 
}

static inline
int64_t control_law(uint32_t interval_us , int64_t t, uint32_t count)
{ 
  assert(t > -1);  
  return t + interval_us / sqrt((double)count); 
}

static inline
void codel_drop(codel_queue_t* q, codel_pkt_t* p)
{
  int64_t now = time_now_us();
  printf("Dropping pkt in CoDel at tstamp = %ld  \n", now);

  assert(q != NULL);
  assert(p != NULL);

  assert(p->data != NULL);
  assert(seq_front(&q->impl) == p);

  void* it_start = seq_at(&q->impl, 0);
  void* it_end = seq_at(&q->impl, 1);

// Fill statistics
  lock_guard(&q->mtx);

  q->bytes -= p->bytes;
  q->pkts -= 1;
  if(q->del != NULL)
    q->del(p->data);
  seq_erase(&q->impl,it_start, it_end);
  q->dropped_pkts += 1;
}

static
void codel_free(queue_t* q_base)
{
  assert(q_base != NULL);
  assert(q_base->size(q_base) == 0);
  codel_queue_t* q = (codel_queue_t*)(q_base);

  void* value_semantic = NULL;
  seq_free(&q->impl, value_semantic );

  mv_avg_wnd_free(&q->avg);

  int rc = pthread_mutex_destroy(&q->mtx);
  assert(rc == 0);

  free(q);
}

static
void codel_push(queue_t* q_base, void* data, size_t bytes)
{
  assert(q_base != NULL);
  assert(q_base->id < 256);
  assert(data != NULL);
  assert(bytes > 0);

  if(q_base->size(q_base) >= (size_t)MAX_CODEL_QUEUE_CAP){
    assert(0 != 0  && "Queue limit reached!!");
    return;
//    return (q_rc_t){.suceed = false, .reason = "Full Queue " }; 
  }


  codel_queue_t* q = (codel_queue_t*)(q_base);
  codel_pkt_t p = {.data = data, .bytes = bytes, .tstamp = time_now_us() }; 
  seq_push_back(&q->impl, (void*)&p, sizeof(codel_pkt_t));

// Fill statistics
  lock_guard(&q->mtx);

  // Statistics
  q->bytes += bytes;
  q->bytes_fwd += bytes;
  q->pkts += 1;
  q->pkts_fwd += 1;


  printf("Codel queue size = %lu %ld \n", q->bytes, time_now_us() );
  
}

static
void codel_pop(queue_t* q_base)
{
  assert(q_base != NULL);
  assert(codel_size(q_base) > 0);
  assert(q_base->id < 256);

  codel_queue_t* q = (codel_queue_t*)(q_base);

  codel_pkt_t* it_start = seq_at(&q->impl, 0);
  assert(q->bytes > 0);

  // Fill statistics
  {
    lock_guard(&q->mtx);

    assert(q->bytes >= it_start->bytes);
    size_t const sz_pkt = it_start->bytes;
    q->bytes -= sz_pkt; 
    assert(q->pkts != 0);
    q->pkts -= 1; //q->sz;

    int64_t const now_us = time_now_us();
    int64_t const ing_us = it_start->tstamp;
    assert(now_us >= ing_us && "Time is a monotonically increasing function" );

    q->last_sojourn_time = now_us - ing_us; 
    mv_avg_wnd_push_back(&q->avg, now_us, q->last_sojourn_time);
  }

  codel_pkt_t* it_end = seq_at(&q->impl, 1);

  size_t const before = seq_size(&q->impl);
  seq_erase(&q->impl, it_start, it_end);
  size_t const after = seq_size(&q->impl);
  assert(after == before - 1);
  size_t const sz = codel_size(q_base); 
  if(sz > 0){
    codel_pkt_t* p = seq_front(&q->impl);
	  assert(p->tstamp != 0);
  }
  printf("CoDel poping from queue number %d tstamp %ld bytes %ld sz %ld \n", q_base->id, time_now_us(), q->bytes, sz);
}


static
void* codel_front(queue_t* q_base)
{
  assert(q_base != NULL);
  assert(q_base->id < 256);

  codel_queue_t* q = (codel_queue_t*)q_base;

  int64_t const now = time_now_us();
  dodequeue_result_t r = dodequeue(q, now); 
  if (q->dropping_) { 
    if (! r.ok_to_drop) {               // sojourn time below TARGET - leave drop state 
      q->dropping_ = false;      
    } 
    // Time for the next drop.  Drop current packet and dequeue next.  If the dequeue doesn’t take us out of dropping
    // state, schedule the next drop.  A large backlog might
    // result in drop rates so high that the next drop should
    // happen now, hence the ’while’ loop.   
    while (now >= q->drop_next_ && q->dropping_) {
      codel_drop(q,r.p);
      ++q->count_; 
      r = dodequeue(q,now); 
      if (! r.ok_to_drop) {                   // leave drop state         
        q->dropping_ = false;
      } else {    
        // schedule the next drop. 
        q->drop_next_ = control_law(q->interval_us, q->drop_next_, q->count_); 
      }  
    } 
    // If we get here, we’re not in drop state.  The ’ok_to_drop’
    // return from dodequeue means that the sojourn time has been    
    // above ’TARGET’ for ’INTERVAL’, so enter drop state.  
  } else if (r.ok_to_drop) {
    codel_drop(q, r.p);  
    r = dodequeue(q,now); 
    q->dropping_ = true;  
    // If min went above TARGET close to when it last went           
    // below, assume that the drop rate that controlled the
    // queue on the last cycle is a good starting point to
    // control it now.  (’drop_next’ will be at most ’INTERVAL’     
    // later than the time of the last drop, so ’now - drop_next’    
    // is a good approximation of the time from the last drop       
    // until now.) Implementations vary slightly here; this is      
    // the Linux version, which is more widely deployed and        
    // tested.  
    uint32_t const delta = q->count_ - q->lastcount_; 
    q->count_ = 1;  
    if ((delta > 1) && (now - q->drop_next_ < 16*q->interval_us ))
      q->count_ = delta;  

    q->drop_next_ = control_law(q->interval_us, now, q->count_); 
    q->lastcount_ = q->count_; 
  } 
  return (r.p->data);
}

static
void* codel_end(queue_t* q_base)
{
  assert(q_base != NULL);
  codel_queue_t* q = (codel_queue_t*)(q_base);

  return seq_end(&q->impl); 
}

static
const char* codel_name(queue_t* q_base)
{
  assert(q_base != NULL);
//  codel_queue_t* q = (codel_queue_t*)(q_base);

  return "CoDel Queue v0.1";
} 

static
tc_queue_t codel_stats(queue_t* q_base)
{
  assert(q_base != NULL);
  codel_queue_t* q = (codel_queue_t*)(q_base);

  tc_queue_t ans = {.type = TC_QUEUE_CODEL}; 
  ans.id = q->base.id;

  tc_queue_codel_t* f = &ans.codel; 

  // Fill statistics
  { 
    lock_guard(&q->mtx);
    //int rc = pthread_mutex_lock(&q->mtx);
    //assert(rc == 0);
    //defer( { rc = pthread_mutex_unlock(&q->mtx); assert(rc == 0); } );

    f->bytes = q->bytes; 
    f->bytes_fwd = q->bytes_fwd;
    f->pkts = q->pkts;
    f->pkts_fwd = q->pkts_fwd;
    f->drp.dropped_pkts = q->dropped_pkts;// Packets dropped in drop state
    f->avg_sojourn_time = mv_avg_wnd_val(&q->avg);
    f->last_sojourn_time = q->last_sojourn_time; 
  }
  return ans;
}

static
void codel_mod(queue_t* q_base, tc_mod_ctrl_queue_t const* mod)
{
  codel_queue_t* q = (codel_queue_t*)(q_base);
  assert(q != NULL);
  assert(mod != NULL);
  assert(mod->type == TC_QUEUE_CODEL);

  lock_guard(&q->mtx);
  
  q->interval_us = mod->codel.interval_ms*1000;
  q->target_us = mod->codel.target_ms*1000;
  printf("Codel new values assigned \n" );
}

queue_t* codel_init(uint32_t id, void (*deleter)(void*))
{
  assert(id < 256 && "not more than 256 queues supported by the moment");

  codel_queue_t* q = malloc(sizeof(codel_queue_t));
  assert(q != NULL);

  q->base.free = codel_free;
  q->base.push = codel_push;
  q->base.pop = codel_pop;
  q->base.size = codel_size;
  q->base.bytes = codel_bytes;
  q->base.front = codel_front;
  q->base.end = codel_end;
  q->base.name = codel_name;
  q->base.handle = NULL;
  q->base.stats = codel_stats;
  q->base.mod = codel_mod;

  q->base.type = TC_QUEUE_CODEL; 

  seq_init(&q->impl, sizeof(codel_pkt_t));

  q->base.id = id;
  q->del = deleter;
  // Statistics
  q->bytes = 0;
  q->bytes_fwd = 0;
  q->pkts = 0;
  q->pkts_fwd = 0;
  q->last_sojourn_time= 0;
  q->dropped_pkts = 0;
  
  double const time_wnd_ms = 100.0;
  mv_avg_wnd_init(&q->avg, time_wnd_ms);

  // CoDel variables
  q->first_above_time_ = 0; // Time to declare sojourn time above TARGET 
  q->drop_next_ = 0;        // Time to drop next packet  
  q->count_ = 0;          // Packets dropped in drop state
  q->lastcount_ = 0;      // Count from previous iteration 
  q->maxpacket_ = 1514;
  q->dropping_ = false;     // Set to true if in drop state

  q->interval_us = 100 * 1000; // 100 ms
  q->target_us = 5 * 1000; // 5 ms


  pthread_mutexattr_t *attr = NULL;
#ifdef DEBUG
  const int type = PTHREAD_MUTEX_ERRORCHECK;
  int rc_mtx = pthread_mutexattr_settype( attr, type); 
  assert(rc_mtx == 0);
#endif
  pthread_mutex_init(&q->mtx, attr);

  return &q->base;
}

