#include "rr_sch.h"

#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "../../alg_ds/ds/seq_container/seq_generic.h"
#include "../../alg_ds/alg/alg.h"

typedef struct rr_s
{
  sch_t base;
  seq_arr_t arr; // queue_t* 
  uint32_t last_idx;
//  void* it_last_q;
} rr_t;

static
void rr_free(sch_t* s_base)
{
  assert(s_base != NULL);
  rr_t* s = (rr_t*)s_base;
  void* reference_semantic = NULL;
  seq_free(&s->arr, reference_semantic);
  free(s);
}

static
void rr_add_queue(sch_t* s_base, queue_t** q)
{
  assert(s_base != NULL);
  rr_t* s = (rr_t*)s_base;
  assert(q != NULL);
  queue_t* tmp = *q;
  assert( tmp->type == TC_QUEUE_FIFO || tmp->type == TC_QUEUE_CODEL ||  tmp->type == TC_QUEUE_ECN_CODEL) ;
  assert(tmp->id < 128 && "Did you added 128 queues?");

  seq_push_back(&s->arr, q, sizeof(queue_t*));
  printf("Adding queue %p to the Scheduler \n", *q );
}



static
bool eq_queue_id(void const* value_v, void const* it)
{
  assert(value_v != NULL);
  assert(it != NULL);

  uint32_t* value = (uint32_t*)value_v;
  queue_t* q = *(queue_t**)it; 

  if(q->id == *value)
    return true;

  return false;
}

static
void rr_del_queue(sch_t* s_base, queue_t** q)
{
  assert(s_base != NULL);
  assert(q != NULL);

  rr_t* s = (rr_t*)s_base;

  void* it = find_if_r(&s->arr, &(*q)->id, eq_queue_id);
  void* end = seq_end(&s->arr);
  assert(it != end && "Queue not found" );
  void* next = seq_next(&s->arr, it); 

  seq_erase(&s->arr, it, next);

  s->last_idx = 0;
}


static
queue_t* rr_next_queue(sch_t* s_base)
{
  assert(s_base != NULL);
  rr_t* s = (rr_t*)s_base;
  uint32_t const sz = seq_size(&s->arr); 
  if(sz == 0)
    return NULL;

  if(s->last_idx >= sz)
    s->last_idx = 0;

  queue_t* q = NULL;
  for(uint32_t i = 0; i < sz; ++i){
      q = *(queue_t**)seq_at(&s->arr, s->last_idx);
      assert(q != NULL);

      s->last_idx += 1; 
      //printf("No pkts at queue id = %d \n", q->id);
       if(s->last_idx >= sz)
           s->last_idx = 0;

      if(q->size(q) != 0){
          printf("Scheduler Next Queue id = %u \n", q->id);
	  return q;
      }
  }

  return NULL;
}

static
void rr_pkt_fwd(sch_t* s_base)
{
  rr_t* s = (rr_t*)s_base;
  (void)s;

//  s->last_idx += 1;
//  if(s->last_idx >= seq_size(&s->arr) ){
//    s->last_idx = 0; 
//  }

//  s->it_last_q = seq_next(&s->arr, s->it_last_q);
 // if(s->it_last_q == seq_end(&s->arr))
 //   s->it_last_q = seq_front(&s->arr);

  //printf("Pkt fwd called \n");
}

static
tc_sch_t rr_stats(sch_t* s_base)
{
  assert(s_base != NULL);
  rr_t* s = (rr_t*)s_base;
  (void)s;

  tc_sch_t ans = {.type = TC_SCHED_RR };
  return ans;
}

static
const char* rr_name(sch_t* sch) 
{
  assert(sch != NULL);
  (void)sch;
  //assert(s_base != NULL);
  //rr_sch_t* s = (rr_sch_t*)s_base;
  return "Round Robin Scheduler";
}

sch_t* rr_init(void)
{
  rr_t * s = malloc(sizeof(rr_t));
  assert(s!=NULL);

  s->base.free = rr_free;
  s->base.add_queue = rr_add_queue;
  s->base.next_queue = rr_next_queue;
  s->base.name = rr_name;
  s->base.pkt_fwd = rr_pkt_fwd;
  s->base.handle = NULL;
  s->base.stats = rr_stats;
  s->base.del_queue =  rr_del_queue;

  seq_init(&s->arr, sizeof(queue_t*));
  s->last_idx = 0;
//  s->it_last_q = seq_end(&s->arr);
  return &s->base;
}


