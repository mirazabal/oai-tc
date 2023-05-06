#include "rr_cls.h"

#include "../../queue/queue.h" 
#include "../../alg_ds/ds/seq_container/seq_generic.h"
#include "../../alg_ds/alg/alg.h"

#include <assert.h>
#include <stdio.h>

typedef struct rr_cls_s
{
  cls_t base;
//  seq_ring_t queues; // queue_t*
  seq_arr_t queues; // queue_t*
  uint32_t last_q;
} rr_cls_t;

static
void rr_cls_free(cls_t* cls_base)
{
  rr_cls_t* cls = (rr_cls_t*)cls_base; 
  void* non_owning = NULL;
  seq_free(&cls->queues, non_owning); 
  free(cls);
}

static
void rr_cls_add(cls_t* cls_base, tc_add_ctrl_cls_t const* cls_data)
{
  assert(0!=0 && "Not implemented!!");
  (void)cls_base;
  (void)cls_data;
}

static
void rr_cls_mod(cls_t* cls_base, tc_mod_ctrl_cls_t const* cls_data)
{
  assert(0!=0 && "Not implemented!!");
  (void)cls_base;
  (void)cls_data;
}

static
void rr_cls_del(cls_t* cls_base, tc_del_ctrl_cls_t const* cls_data)
{
  assert(0!=0 && "Not implemented!!");
  (void)cls_base;
  (void)cls_data;
}


static
void rr_cls_add_queue(struct cls_s* cls_base, queue_t** q)
{
  assert(cls_base != NULL);
  assert(q != NULL);
  rr_cls_t* cls = (rr_cls_t*)cls_base;
  seq_push_back(&cls->queues, q, sizeof(queue_t*));
  printf("Adding queue to the clasifier \n");
};

static
bool eq_queue_pointer(void const* value_v, void const* it)
{
  assert(value_v != NULL);
  assert(it != NULL);

  queue_t* q0 = *(queue_t**)value_v;
  queue_t* q1 = *(queue_t**)it;

  return q0 == q1;
}

static
void rr_cls_del_queue(struct cls_s* cls_base, queue_t** q)
{
  assert(cls_base != NULL);
  assert(q != NULL);
  rr_cls_t* cls = (rr_cls_t*)cls_base;

  void* it = find_if_r(&cls->queues, q, eq_queue_pointer);
  void* end = seq_end(&cls->queues);
  assert(it != end && "Queue not found");

  void* next = seq_next(&cls->queues, it);
  seq_erase(&cls->queues, it, next );
  
  cls->last_q = 0; // To avoid race conditions
};


static
queue_t* rr_cls_dst_queue(cls_t* cls_base, const uint8_t* data, size_t size)
{
  assert(data != NULL);
  assert(size != 0);

  rr_cls_t* cls = (rr_cls_t*)cls_base;

  //printf("Last queue number RR %d \n", cls->last_q);
  void* it_q = seq_at(&cls->queues, cls->last_q);
  assert(it_q != NULL);
  queue_t* q = *(queue_t**)it_q; 
  assert(q != NULL);
  return q; // *(queue_t**)seq_at(&cls->queues, pos);
}

static
void rr_cls_pkt_fwd(cls_t* cls_base)
{
  rr_cls_t* cls = (rr_cls_t*)cls_base;

  const uint32_t num_queues = seq_size(&cls->queues);
  //printf("num_queues = %d \n", num_queues);
  if(num_queues == 0)
    return;

  assert(cls->last_q < num_queues);
  if(cls->last_q + 1 == num_queues)
    cls->last_q = 0;
  else
    cls->last_q += 1;
}

static
tc_cls_t rr_cls_stats(cls_t* cls_base)
{
  assert(cls_base != NULL);
  rr_cls_t* cls = (rr_cls_t*)cls_base;
  (void)cls;

  tc_cls_t ans = {.type = TC_CLS_RR };
  ans.rr.dummy = 42;
  return ans;
}

static
const char* rr_cls_name(cls_t* cls)
{
  (void)cls;
  return "Round Robin Classifier";
}

cls_t* rr_cls_init(void)
{
  rr_cls_t* cls = malloc(sizeof(rr_cls_t));
  assert(cls != NULL);

  cls->base.dst_q = rr_cls_dst_queue;
  cls->base.name = rr_cls_name;
  cls->base.handle = NULL;
  cls->base.free = rr_cls_free;
  cls->base.add_queue = rr_cls_add_queue;
  cls->base.del_queue = rr_cls_del_queue;
  cls->base.pkt_fwd = rr_cls_pkt_fwd;
  cls->base.stats = rr_cls_stats;
  
  cls->base.add = rr_cls_add;
  cls->base.mod = rr_cls_mod;
  cls->base.del = rr_cls_del;

  cls->last_q  = 0;
  seq_init(&cls->queues, sizeof(queue_t*));

  printf("RR cls init \n");
  return (cls_t*)&cls->base; 
} 

