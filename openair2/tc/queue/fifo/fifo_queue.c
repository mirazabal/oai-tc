/*
 * Array based thread safe queue implementation
 */
#include "fifo_queue.h"
#include "../../alg_ds/alg/defer.h"
#include "../../alg_ds/ds/statistics/moving_average/mv_avg_time.h"
#include "../../alg_ds/ds/lock_guard/lock_guard.h"
#include "../../time/time.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
//#include <threads.h> C11 library
#include <time.h>
#include <unistd.h>

// __cleanup__ attributte as well as GLib library 
#define QUEUE_MAX_CAP_NAIVE 163840

typedef struct
{
  int64_t tstamp;
  uint8_t* data;
  uint32_t bytes; 
} pkt_t;

typedef struct queue_arr_s
{
  queue_t base;
  pkt_t val[QUEUE_MAX_CAP_NAIVE]; 
  int32_t front;
  int32_t rear;
  uint32_t bytes;
  uint32_t bytes_fwd;
  uint32_t pkts;
  uint32_t pkts_fwd;
  int64_t last_sojourn_time;
  mv_avg_wnd_t avg; 

  pthread_mutex_t mtx;
//  float avg_sojourn_time;
} queue_arr_t;


static
void fifo_free(queue_t* q_base)
{
  assert(q_base != NULL);
  assert(q_base->size(q_base) == 0);

  queue_arr_t* q = (queue_arr_t*)q_base;

  mv_avg_wnd_free(&q->avg);

  int rc = pthread_mutex_destroy(&q->mtx);
  assert(rc == 0);

  free(q);
}

static
void fifo_push(queue_t* q_base, void* data, size_t bytes)
{
  queue_arr_t* q = (queue_arr_t*)q_base;
  assert(q != NULL);
  assert(data != NULL);

  if(q->rear == -1){ // drop the packet
    assert(0!= 0 && "Queue limit reached!!!");
    return;
  }

  assert(q->rear < QUEUE_MAX_CAP_NAIVE && q->rear > -1);
  const int64_t tstamp =  time_now_us();
  q->val[q->rear] = (pkt_t) {.data = data, .bytes = bytes, .tstamp = tstamp};

  if(q->front == -1){
    q->front = q->rear;
  }

  assert(q->front > -2);
  if(q->rear == QUEUE_MAX_CAP_NAIVE - 1){
    q->rear = 0;
  } else { 
    ++q->rear;
  }

  if(q->rear == q->front) 
    q->rear = -1;

  // Fill statistics
  {
    lock_guard(&q->mtx); 

    q->bytes += bytes;
    q->bytes_fwd += bytes;

    q->pkts += 1;
    q->pkts_fwd += 1;
  }

  assert(q->bytes >= bytes && "Overflow detected");

//  printf("Ingressing pkt into FIFO queue number = %u \n", q_base->id );
}

static
void fifo_pop(queue_t* q_base)
{
  queue_arr_t* q = (queue_arr_t*)q_base;
  assert(q != NULL);

  if(q->front == -1){
    return;
  }

  assert(q->front <  QUEUE_MAX_CAP_NAIVE && q->front > -2);
  pkt_t p = q->val[q->front]; 
  q->val[q->front] = (pkt_t){.data = NULL, .bytes =0, .tstamp = 0} ;

  if(q->rear == -1)
    q->rear = q->front;

  if(q->front == QUEUE_MAX_CAP_NAIVE - 1)
    q->front = 0;
  else
    ++q->front;

  if(q->front == q->rear)
    q->front = -1;


  int64_t const tstamp = time_now_us(); 

  printf("FIFO poping from queue number %d elapsed_time = %ld num_pkts = %d \n", q_base->id, tstamp - p.tstamp, q->pkts);
  // Fill statistics
  {
    lock_guard(&q->mtx); 

    q->bytes -= p.bytes;
    assert(tstamp >= p.tstamp && "Time is a monotonically increasing function");
    q->last_sojourn_time = tstamp - p.tstamp; 

    mv_avg_wnd_push_back(&q->avg,tstamp, q->last_sojourn_time);

    assert(q->pkts != 0);
    q->pkts -= 1;
  }

}

static
size_t fifo_size(queue_t* q_base)
{
  queue_arr_t* q = (queue_arr_t*)q_base;
  assert(q != NULL);
//  int rc = pthread_rwlock_rdlock(&q->rwlock);
//  assert(rc == 0);
  int64_t ret_val = 0;
  if(q->front == -1){
    ret_val = 0;  
  } else if (q->rear == -1){
    ret_val = QUEUE_MAX_CAP_NAIVE;
  } else if (q->rear > q->front){
    ret_val = q->rear - q->front;
  } else {
    ret_val = q->rear + QUEUE_MAX_CAP_NAIVE - 1 - q->front;
  }
  assert(ret_val > -1);
//  rc = pthread_rwlock_unlock(&q->rwlock);
//  assert(rc == 0);

  return ret_val;
//  size_t ret_val_s = ret_val;
//  assert(ret_val_s == ret_val);
//  return ret_val_s;
}

static
size_t fifo_bytes(queue_t* q_base)
{
  queue_arr_t* q = (queue_arr_t*)q_base;
  assert(q != NULL);
  return q->bytes; 
}

static
void* fifo_front(queue_t* q_base)
{
  queue_arr_t* q = (queue_arr_t*)q_base;
  assert(q != NULL);

  if (q->front == -1)
    return NULL;

  const pkt_t ret_val = q->val[q->front];
  return ret_val.data;
}

static
void* fifo_end(queue_t* q_base)
{
  queue_arr_t* q = (queue_arr_t*)q_base;
  assert(q != NULL);
  return &q->rear; 
}

/*
void* fifo_at(queue_t* q_base, uint32_t pos)
{
  queue_arr_t* q = (queue_arr_t*)q_base;
  assert(q != NULL);

//  assert(steps > 0 && "Zero or negative steps not implemented");
  assert(pos < QUEUE_MAX_CAP_NAIVE && "Asking to forward more steps than the queue capacity");

  pkt_t rc = {.data = NULL, .bytes =0 };
  if(q->front == -1){
    //rc = NULL;
  } else if(q->front + pos < QUEUE_MAX_CAP_NAIVE){
    rc = q->val[q->front + pos];
  } else {
    const int diff = q->front + pos - QUEUE_MAX_CAP_NAIVE;
    rc = q->val[diff]; 
  }
  return rc.data;
}
*/

static
const char* fifo_name(queue_t* q)
{
  assert(q != NULL);
  return "FIFO queue";
} 

static
tc_queue_t fifo_stats(queue_t* q_base)
{
  queue_arr_t* q = (queue_arr_t*)q_base;
  assert(q != NULL);

  tc_queue_t ans = {.type = TC_QUEUE_FIFO}; 
  ans.id = q->base.id;

  tc_queue_fifo_t* f = &ans.fifo; 

  // Fill statistics
  {
    lock_guard(&q->mtx);

    f->bytes = q->bytes; 
    f->bytes_fwd = q->bytes_fwd;
    f->pkts = q->pkts;
    f->pkts_fwd = q->pkts_fwd;
    f->drp.dropped_pkts = 0;
    f->avg_sojourn_time = mv_avg_wnd_val(&q->avg);
    f->last_sojourn_time = q->last_sojourn_time; 
  }
  return ans;
} 

static
void fifo_mod(queue_t* q_base, tc_mod_ctrl_queue_t const* mod)
{
  queue_arr_t* q = (queue_arr_t*)q_base;
  assert(q != NULL);
  assert(mod != NULL);

  (void)q;
  (void)mod;
  assert(0!=0 && "Not implemented");
}

queue_t* fifo_init(uint32_t id, void (*deleter)(void*))
{
  assert(id < 256 && "Not more than 256 queues supported for the moment");

  assert(deleter == NULL);
  queue_arr_t* q = malloc(sizeof(queue_arr_t)); 
  assert(q != NULL && "Memory exhausted");
  q->rear=0; // 
  q->front=-1; // cannot dequeue 

  q->bytes_fwd = 0;
  q->pkts_fwd = 0;

  memset(q->val, 0, sizeof( pkt_t ) * QUEUE_MAX_CAP_NAIVE );

  q->last_sojourn_time = 0;

  const float time_window_ms = 100.0;
  mv_avg_wnd_init(&q->avg, time_window_ms);

  q->base.free = fifo_free;
  q->base.push = fifo_push;
  q->base.pop = fifo_pop;
  q->base.size = fifo_size;
  q->base.bytes = fifo_bytes;
  q->base.front = fifo_front;
  q->base.end = fifo_end;
  //q->base.at = fifo_at;
  q->base.name = fifo_name;
  q->base.handle = NULL;
  q->base.id = id;
  q->base.stats = fifo_stats;
  q->base.mod = fifo_mod;

  q->base.type = TC_QUEUE_FIFO; 

  pthread_mutexattr_t *attr = NULL;
#ifdef DEBUG
  const int type = PTHREAD_MUTEX_ERRORCHECK;
  int rc_mtx = pthread_mutexattr_settype(attr, type); 
  assert(rc_mtx == 0);
#endif
  pthread_mutex_init(&q->mtx, attr);

  return &q->base;
}

