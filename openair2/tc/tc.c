#include "alg_ds/alg/alg.h"
#include "alg_ds/alg/defer.h"
#include "alg_ds/ds/lock_guard/lock_guard.h"
#include "tc.h"
#include "pkt.h"
#include "plc_shp_pair.h"

#include "time/time.h"

#include <assert.h>
#include <dlfcn.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>


#define QUOTE(str) #str
#define EXPAND_AND_QUOTE(str) QUOTE(str)
#define TC_SRC_DIR EXPAND_AND_QUOTE(TC_CURRENT_SOURCE_DIR)

static
atomic_size_t queue_id = 0;

static inline
bool cmp_func_ht(const void* a_v, const void* b_v)
{
  uintptr_t* a = (uintptr_t*)a_v;
  uintptr_t* b = (uintptr_t*)b_v;
  return *a == *b; 
}

static
void free_func_ht(void* key, void* value)
{
  assert(key != NULL);
  assert(value != NULL);

  free(key);
  plc_shp_pair_t* pair = (plc_shp_pair_t*)value; //  retval->data; 
  free_plc_shp(pair);
}

static
void free_alloc_queues(void* it)
{
  assert(it != NULL);
  queue_t* q = *(queue_t**)it;
  while(q->size(q) != 0){
    pkt_t* next_pkt = q->front(q); 
    q->pop(q);
    assert(next_pkt != NULL);
    //free(next_pkt->data);
    free_pkt(next_pkt);
  }

  void* handle = q->handle;
  q->free(q);
  int const rc = dlclose(handle);
  assert(rc == 0);
}

static
void free_plc_q_shp(tc_t* tc)
{
  assoc_free(&tc->htab);
  seq_free(&tc->queues, free_alloc_queues);
}

static
void free_sch(sch_t* sch)
{
  assert(sch->handle != NULL);
  void* handle = sch->handle;
  sch->free(sch);
  int const rc = dlclose(handle);
  assert(rc == 0);
}

static
void free_pcr(pcr_t* pcr)
{
  assert(pcr->handle != NULL);
  void* handle = pcr->handle;
  pcr->free(pcr);
  int const rc = dlclose(handle);
  assert(rc == 0);
}

static
void free_alloc_clss(void* it)
{
  assert(it != NULL);
  cls_t* cls = *(cls_t**)it;
  void* handle = cls->handle;
  cls->free(cls);
  int const rc = dlclose(handle);
  assert(rc == 0);
}

static 
void free_clss(tc_t* tc)
{
  seq_free(&tc->clss, free_alloc_clss );
}

static inline
bool cls_pkt_matches_queue(const void* p_v, const void* cls_v )
{
  pkt_t* p = (pkt_t*)p_v;
  cls_t* c = *(cls_t**)cls_v;
  queue_t* q = c->dst_q(c, p->data, p->sz);
  return q != NULL;
}

typedef struct{
  cls_t* cls;
  queue_t* q;
} cls_q_t;

static
cls_q_t cls_select_q(tc_t* tc, pkt_t* p)
{
  assert(tc != NULL);
  assert(p != NULL);

  // Default queue 0
  queue_t* q = seq_at(&tc->queues,0);
  assert(q != NULL && "Queue not init");
  void* end = seq_end(&tc->clss);
  cls_q_t ret = {.cls = end, .q = q }; 

  void* it = find_if_r(&tc->clss, p, cls_pkt_matches_queue);
  if(it != end){
    void* first = seq_front(&tc->clss);
    ptrdiff_t const pos = seq_distance(&tc->clss, first, it); 
    ret.cls = *(cls_t**)seq_at(&tc->clss, pos);
    assert(ret.cls->dst_q != NULL);
    ret.q = ret.cls->dst_q(ret.cls, p->data, p->sz);
  }
  return ret;
}

typedef struct{
  plc_t* plc;
  queue_t* q;
  plc_act_e action; 
} plc_q_t;

static
plc_q_t plc_select_q(tc_t* tc, queue_t* q, pkt_t* p)
{
  assert(tc != NULL);
  assert(q != NULL);
  assert(p != NULL);

  plc_shp_pair_t* pair = find_plc_shp(&tc->htab, &q);
  assert(pair != NULL);

  plc_q_t ret = {.plc = pair->plc, .q = q, .action = PLC_DROP }; 

  ret.action = plc_action(ret.plc, p->sz);
  assert(ret.action ==  PLC_DROP 
         || ret.action == PLC_PASS 
         || ret.action == PLC_DEVIATE );

  if(ret.action == PLC_DEVIATE)
    ret.q = ret.plc->dev_q;

  return ret;
}

void tc_free(tc_t* tc)
{
  free_clss(tc);

  free_plc_q_shp(tc);

  free_sch(tc->sch);

  free_pcr(tc->pcr);

  int const rc = pthread_mutex_destroy(&tc->mtx);
  assert(rc == 0);

  free(tc);
}

void tc_init(tc_t* tc, uint32_t rb_id, uint32_t rnti)
{
  printf("tc_init \n");
  assert(tc != NULL);
  assert(rb_id < 30);

  tc_t tmp = {.rb_id = rb_id, .rnti = rnti};
  memcpy(tc, &tmp, sizeof(tc_t));

  seq_init(&tc->clss, sizeof(cls_t*));

  seq_init(&tc->queues, sizeof(queue_t*));

  pthread_mutexattr_t *attr = NULL;

#if DEBUG
  *attr = PTHREAD_MUTEX_ERRORCHECK;
#endif

  int const rc = pthread_mutex_init(&tc->mtx, attr );
  assert(rc == 0);
}

static
void check_dl_error(void)
{
  const char* error = dlerror();
  if (error != NULL) {
    printf("Error from DL = %s \n", error);
    fflush(stdout);
    assert(0 != 0 && "error loading the init of the shared object");
  }
}

static
void load_cls(tc_t* tc, queue_t* q, const char* file_path, const char* init_func)
{
  assert(tc != NULL);
  assert(q != NULL);
  assert(file_path != NULL);
  assert(init_func != NULL );

  printf("Load classifier file path = %s \n", file_path );
  fflush(stdout);

  void* handle = dlopen(file_path, RTLD_NOW);
  assert(handle != NULL && "Could not open the file path");
  dlerror();    /* Clear any existing error */
  cls_t* (*fp)(void);
  fp = dlsym(handle, init_func);
  check_dl_error();
  cls_t* c = fp();

  c->handle = handle; 
  c->add_queue(c, &q);
  assert(c->dst_q != NULL);
  seq_push_back(&tc->clss, &c, sizeof(cls_t*)); 
}

static 
void load_sch(tc_t* tc, const char* file_path, const char* init_func)
{

  printf("Load scheduler file path = %s \n", file_path );
  void* handle = dlopen(file_path, RTLD_NOW);
  if(handle == NULL)
	  printf("Error opening %s \n",dlerror() );

  fflush(stdout);
  assert(handle != NULL && "Could not open the file path");
  dlerror();    /* Clear any existing error */
  sch_t* (*fp)(void);
  fp = dlsym(handle, init_func);
  check_dl_error();
  sch_t* s = fp();
  tc->sch = s;
  tc->sch->handle = handle;
}

static
void load_pcr(tc_t* tc, const char* file_path, const char* init_func)
{
  assert(tc != NULL);
  assert(file_path != NULL);
  assert(init_func != NULL);

  printf("Load pacer file path = %s \n", file_path );
  fflush(stdout);
 

  if(tc->pcr != NULL && tc->pcr->handle != NULL)
    free_pcr(tc->pcr);

  void* handle = dlopen(file_path, RTLD_NOW);
  if(handle == NULL)
    printf ("Error loading the lib = %s \n", dlerror());
  assert(handle != NULL && "Could not open the file path");
  dlerror();    // Clear any existing error 
  pcr_t* (*fp)(void);
  fp = dlsym(handle, init_func);
  check_dl_error();
  pcr_t* p = fp();
  tc->pcr = p;
  tc->pcr->handle = handle;
}

static
queue_t* load_queue(tc_t* tc, const char* file_path, const char* init_func)
{

  printf("Load queue file path = %s \n", file_path );
  fflush(stdout);
 
  // Create the FIFO default queue at position 0
  void* handle = dlopen(file_path, RTLD_NOW);
  if(handle == NULL){
    const char* str = dlerror(); 
    printf("%s \n", str );
  }
  assert(handle != NULL && "Could not open the file path");
  dlerror();    /* Clear any existing error */
  queue_t* (*fp)(uint32_t, void (*deleter)(void*) );
  fp = dlsym(handle, init_func);
  check_dl_error();
  printf("queue_id = %lu\n", queue_id);
  fflush(stdout);
  queue_t* q = fp(queue_id, NULL);
  ++queue_id;

  q->handle = handle;
  seq_push_back(&tc->queues, &q, sizeof(queue_t*));
  return q;
}

// Map queue to policer and shaper
static
void map_q_plc_shp(assoc_ht_open_t* htab, queue_t** q, plc_t** p, shp_t** s)
{
  assert(htab!=NULL);
  assert(q != NULL);
  assert(p != NULL);
  assert(s != NULL);
  
  plc_shp_pair_t* pair = init_pcr_shp(p,s);
  assert(pair != NULL);
  const uintptr_t key = (uintptr_t)(*q);
  printf("Queue pointer KEY VALUE = %ld \n", key);

  assoc_insert(htab, &key, sizeof(uintptr_t), pair);
}

static
void del_q_plc_shp(assoc_ht_open_t* htab, queue_t** q)// queue, policer and shaper are saved together, and thus deleted together  
{
  assert(htab != NULL);
  assert(q != NULL);

  uintptr_t key = (uintptr_t)(*q);
  plc_shp_pair_t* p = (plc_shp_pair_t*)assoc_extract(htab, &key);
  plc_free(p->plc);
  shp_free(p->shp);
  free(p);
} 


static
pcr_act_e check_pcr_action(pcr_t* p, pkt_t* next_pkt)
{
  return p->action(p, next_pkt->sz); 
}

void tc_ingress_pkts(tc_t* tc, uint8_t* data, size_t sz)
{
  assert(tc != NULL);
  assert(data != NULL);
  assert(sz != 0);

  int64_t now = time_now_us();

  lock_guard(&tc->mtx);

  pkt_t* p = init_pkt(data, sz);

  // Classifier -> output the queue
  cls_q_t cls_q = cls_select_q(tc, p); 

  // Policer -> output the queue
  plc_q_t plc_q = plc_select_q(tc, cls_q.q, p);

  if(plc_q.action == PLC_DROP){
    printf("Policer dropping the packet \n");
    free_pkt(p);
    return ;
  } 

//  printf("Pushing into queue id = %d\n", plc_q.q->id);

  // Ingress the packet in the queue
  plc_q.q->push(plc_q.q, p, sz);

  // Inform the plc and the cls that the pkt was fwd
  plc_bytes_fwd(plc_q.plc, sz);

  // inform the classifier
  if(cls_q.cls != NULL)
   cls_q.cls->pkt_fwd(cls_q.cls); 

  printf("TC Ingress delay %ld \n", time_now_us() - now);
}

//static
//int counter = 0;

void tc_egress_pkts(tc_t* tc)
{
  assert(tc != NULL);

  if(tc == NULL) return;

  lock_guard(&tc->mtx);

  assert(tc->sch != NULL);
  assert(tc->pcr != NULL);
  assert(seq_size(&tc->queues) > 0);

  sch_t* sch = tc->sch;
  for(;;){
    int64_t now = time_now_us(); 
    // Scheduler -> output from which queue
    queue_t* q = sch->next_queue(sch); 
    if(q == NULL || q->size(q) == 0) {
      //printf("Not egresing, queue size == 0\n");
      break;
    }

    pkt_t* next_pkt = q->front(q); 
    if(next_pkt == NULL){ 
      printf("Not egresing, next_pkt == NULL\n");
      break; // case of CoDel
    }
    assert(next_pkt != NULL);

    // Shaper 
    plc_shp_pair_t* pair = find_plc_shp(&tc->htab, &q);
    assert(pair != NULL);
    shp_t* shp = pair->shp;
    shp_act_e s_act = SHP_PASS; 
    if(q->id == 2){
     float const bnd = mtr_bndwdth_kbps(&shp->m);
     double const wnd_ms = shp->m.avg_wnd.wnd_ms;
     printf("queue id 2 max_rate_kbps %d active %d bndwdth_kbps %f wnd_ms %lf \n", shp->max_rate_kbps, shp->active, bnd, wnd_ms ); // a shaper per queue always exists
    }
    if(shp->active == true){ 
      s_act = shp_action(shp, next_pkt->sz);
      if(s_act == SHP_WAIT){
        printf("Not egresing, Shaper waiting \n ");
        printf("Shaper waiting \n");
        break;
      }
    }

    // Pacer
    pcr_t* pcr = tc->pcr;
    pcr_act_e p_act = check_pcr_action(pcr, next_pkt);
    if(p_act == PCR_WAIT){
      printf("Not egresing, Pacer waiting \n" );
      break; // common to all queues
    }

    // The shaper and the pacer agree to forward the pkt
    assert(s_act == SHP_PASS && p_act == PCR_PASS);

    // Inform the shaper 
    if(shp->active){
      shp_bytes_fwd(shp, next_pkt->sz); 
    } 
    //printf("Forwarding the packet \n");
    // Inform the pacer
    pcr->bytes_fwd(tc->pcr, next_pkt->sz);

    // Inform the scheduler
    sch->pkt_fwd(sch); 

    // Pop the packet from the queue
    assert(q->size(q) > 0); 

    //size_t const sz_before = q->size(q);
    q->pop(q);
    //size_t const sz_after = q->size(q);

    //assert(sz_after == sz_before -1);

    tc->egress_fun(tc->rnti, tc->rb_id, next_pkt->data, next_pkt->sz);

    printf("TC Egress delay %ld \n", time_now_us() - now);

    free_pkt(next_pkt);
    //printf("Dequeuing packet from queue id = %d with size = %lu \n", q->id ,q->size(q) );
    //free(next_pkt->data);
    //printf("Freeing the packet number = %d\n", counter);
    //counter += 1;
  }
}

static
void add_queue_to_cls(const void* it, const void* data)
{
  assert(it != NULL);
  assert(data != NULL);

  cls_t* cls = *(cls_t**)it;
  queue_t* q = *(queue_t**)data;
  cls->add_queue(cls, &q);
}

static
void del_queue_to_cls(const void* it, const void* data)
{
  assert(it != NULL);
  assert(data != NULL);

  cls_t* cls = *(cls_t**)it;
  queue_t* q = *(queue_t**)data;
  cls->del_queue(cls, &q);
}




////
// Configuration management i.e., ADD, MOD, DEL
///


/////////
/// CLASSIFIER ADD/MOD/DEL
/////////

static
tc_rc_t tc_add_cls(tc_t* tc, tc_add_ctrl_cls_t const* add)
{
  assert(tc != NULL);
  assert(add != NULL);

  assert(seq_size(&tc->clss) == 1 && "Only one cls supported");

  cls_t* c = *(cls_t**) seq_front(&tc->clss);

  lock_guard(&tc->mtx);
  c->add(c, add);

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

static
tc_rc_t tc_del_cls(tc_t* tc, tc_del_ctrl_cls_t const* del)
{
  assert(tc != NULL);
  assert(del != NULL);

  assert(0!=0 && "Not implemented");

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

static
tc_rc_t tc_mod_cls(tc_t* tc, tc_mod_ctrl_cls_t const* mod)
{
  assert(tc != NULL);
  assert(mod != NULL);
  assert(seq_size(&tc->clss) == 1 && "Only one cls supported");

  cls_t* c = *(cls_t**) seq_front(&tc->clss);

  lock_guard(&tc->mtx);
  c->mod(c, mod);

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}


/////////
/// POLICER ADD/MOD/DEL
/////////

static
tc_rc_t tc_add_plc(tc_t* tc, tc_add_ctrl_plc_t const* add)
{
  assert(tc != NULL);
  assert(add != NULL);

  assert(0!=0 && "Not implemented");

  return (tc_rc_t){.has_value = true, .tc = tc}; 
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
tc_rc_t tc_mod_plc(tc_t* tc, tc_mod_ctrl_plc_t const* mod)
{
  assert(tc != NULL);
  assert(mod != NULL);

  lock_guard(&tc->mtx);

  uint32_t q_id = mod->id;
  void* it = find_if_r(&tc->queues, &q_id, eq_queue_id);
  void* end = seq_end(&tc->queues);
  assert(it != end && "ID not found");

  queue_t* q = *(queue_t**)it;

  uint32_t q_dev_id = mod->dev_id;
  it = find_if_r(&tc->queues, &q_dev_id, eq_queue_id);
  end = seq_end(&tc->queues);
  assert(it != end && "ID not found");

  queue_t* q_dev = *(queue_t**)it;

  plc_shp_pair_t* pair = find_plc_shp(&tc->htab, &q);
  assert(pair != NULL);

  plc_mod(pair->plc, &q_dev, mod);

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

static
tc_rc_t tc_del_plc(tc_t* tc, tc_del_ctrl_plc_t const* del)
{
  assert(tc != NULL );
  assert(del != NULL);

  assert(0!=0 && "Not implemented");

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}





/////////
/// QUEUE ADD/MOD/DEL
/////////

static
void tc_add_q_impl(tc_t* tc, const char* file_path, const char* init_func)
{
/*
  void* handle = dlopen(file_path, RTLD_NOW);
  if(handle == NULL){
    const char* str = dlerror(); 
    printf("%s \n", str );
  }
  assert(handle != NULL && "Could not open the file path");
  dlerror();    // Clear any existing error 
  queue_t* (*fp)(uint32_t id ,void (*deleter)(void*) );
  fp = dlsym(handle, init_func);
  check_dl_error();
  queue_t* q = fp(queue_id,free_pkt);
  ++queue_id;
  assert(q != NULL);
  q->handle = handle;

  assert(q->id < 128 && "128 queues added or bug?");
  assert( queue_id < 128 && "128 queues added or bug?");
*/

  lock_guard(&tc->mtx);

  queue_t* q = load_queue(tc, file_path, init_func);

  // create policer 
  const uint32_t drop_rate_kbps = 2000000;
  const uint32_t dev_rate_kbps =  2500000;
  const uint32_t time_window_us = 100000; 

  plc_t* plc = plc_init(drop_rate_kbps, dev_rate_kbps, time_window_us, q, q);

  // add queue to the scheduler
  tc->sch->add_queue(tc->sch, &q);

  // create the shaper
  shp_t* shp = shp_init(time_window_us, drop_rate_kbps); 

  map_q_plc_shp(&tc->htab, &q, &plc, &shp);// queue, policer and shaper are saved together  

  for_each(&tc->clss, seq_front(&tc->clss), seq_end(&tc->clss), add_queue_to_cls, &q);
}






static
tc_rc_t tc_add_q(tc_t* tc, tc_add_ctrl_queue_t const* add)
{
  assert(tc != NULL);
  assert(add != NULL);

  if(add->type == TC_QUEUE_CODEL) {
    const char* file_path = TC_SRC_DIR "/build/queue/libcodel_queue.so"; 
    const char* init_func = "codel_init";
    tc_add_q_impl(tc, file_path, init_func);
  } else if(add->type == TC_QUEUE_FIFO) {
    const char* file_path = TC_SRC_DIR "/build/queue/libfifo_queue.so"; 
    const char* init_func = "fifo_init";
    tc_add_q_impl(tc, file_path, init_func);
  } else if(add->type == TC_QUEUE_ECN_CODEL) {
    const char* file_path = TC_SRC_DIR "/build/queue/libecn_codel_queue.so"; 
    const char* init_func = "ecn_codel_init";
    tc_add_q_impl(tc, file_path, init_func);
  } else {
    assert(0!=0 && "Unknwon queue type");
  }

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

static
tc_rc_t tc_mod_q(tc_t* tc, tc_mod_ctrl_queue_t const* ctrl)
{
  assert(tc != NULL);
  assert(ctrl != NULL);

  lock_guard(&tc->mtx);

  uint32_t q_id = ctrl->id;
  void* it = find_if_r(&tc->queues, &q_id, eq_queue_id);
  void* end = seq_end(&tc->queues);
  assert(it != end && "ID not found");

  queue_t* q = *(queue_t**)it;
  assert(q->type == ctrl->type && "Queue type mismatch");

  q->mod(q,ctrl);

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

/*
static
void empty_queue(queue_t* q)
{
  // Empty the queue
  size_t const sz = q->size(q); 
  while(q->size(q) != 0 ){
    pkt_t* p = q->front(q);
    if(p != NULL){
      free_pkt(p);
      q->pop(q);
    }
  }
  assert(q->size(q) == 0); 
}
*/

static
void tc_del_q_impl(tc_t* tc, uint32_t q_id, tc_queue_e type)
{
  assert(tc != NULL);

  lock_guard(&tc->mtx);

  void* it = find_if_r(&tc->queues, &q_id, eq_queue_id);
  void* end = seq_end(&tc->queues);
  assert(it != end && "ID not found");

  queue_t* q = *(queue_t**)it;
  assert(q->type == type && "Queue type mismatch");

  // del queue from the scheduler
  tc->sch->del_queue(tc->sch, it);

  del_q_plc_shp(&tc->htab, it);// queue, policer and shaper are saved together, and thus deleted together  

  for_each(&tc->clss, seq_front(&tc->clss), seq_end(&tc->clss), del_queue_to_cls, &q);

  void* next = seq_next(&tc->queues, it); 
  seq_erase(&tc->queues, it, next );

  void* it_assert = find_if_r(&tc->queues, &q_id, eq_queue_id);
  void* end_assert = seq_end(&tc->queues);
  assert(it_assert == end_assert && "Queue ID not deleted???");

  free_alloc_queues(&q);
}

static
tc_rc_t tc_del_q(tc_t* tc, tc_del_ctrl_queue_t const* del)
{
  assert(tc != NULL);
  assert(del != NULL);

  tc_del_q_impl(tc, del->id, del->type);

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}


/////////
/// SCHEDULER ADD/MOD/DEL
/////////

static
tc_rc_t tc_add_sch(tc_t* tc, tc_add_ctrl_sch_t const* add)
{
  assert(tc != NULL);
  assert(add != NULL);
  
  assert(0!=0 && "not implemented");

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

static
tc_rc_t tc_del_sch(tc_t* tc,  tc_del_ctrl_sch_t const* del)
{
  assert(tc != NULL);
  assert(del != NULL);

  assert(0!=0 && "not implemented");
  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

static
tc_rc_t tc_mod_sch(tc_t* tc, tc_mod_ctrl_sch_t const* mod)
{
  assert(tc != NULL);
  assert(mod != NULL);

  assert(0!=0 && "not implemented");
  return (tc_rc_t){.has_value = true, .tc = tc}; 
}


/////////
/// SHAPER ADD/MOD/DEL
/////////

static
tc_rc_t tc_add_shp(tc_t* tc, tc_add_ctrl_shp_t const* add)
{
  assert(tc != NULL);
  assert(add != NULL);


  assert(0!=0 && "not implemented");
  return (tc_rc_t){.has_value = true, .tc = tc}; 
}


static
tc_rc_t tc_del_shp(tc_t* tc, tc_del_ctrl_shp_t const* del )
{
  assert(tc != NULL);
  assert(del != NULL);


  assert(0!=0 && "not implemented");
  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

static
tc_rc_t tc_mod_shp(tc_t* tc, tc_mod_ctrl_shp_t const* mod)
{
  assert(tc != NULL);
  assert(mod != NULL);

  lock_guard(&tc->mtx);

  uint32_t q_id = mod->id;
  void* it = find_if_r(&tc->queues, &q_id, eq_queue_id);
  void* end = seq_end(&tc->queues);
  assert(it != end && "ID not found");

  queue_t* q = *(queue_t**)it;

  plc_shp_pair_t* pair = find_plc_shp(&tc->htab, &q);
  assert(pair != NULL);

  shp_t* dst = pair->shp; 
  shp_mod(dst, mod);

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}


/////////
/// PACER ADD/MOD/DEL
/////////


tc_rc_t tc_add_pcr(tc_t* tc, tc_add_ctrl_pcr_t const* add)
{
  assert(tc != NULL);
  assert(add != NULL);

  assert(0!=0 && "Not implemented");
  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

tc_rc_t tc_mod_pcr(tc_t* tc , tc_mod_ctrl_pcr_t const* mod)
{
  assert(tc != NULL);
  assert(mod != NULL);

  lock_guard(&tc->mtx);

  if(mod->type == tc->pcr->type){
    tc->pcr->mod(tc->pcr, mod);
  } else if(mod->type == TC_PCR_DUMMY){
    const char* pcr_file_path = TC_SRC_DIR "/build/pcr/libdummy_pcr.so";
    const char* pcr_init_func = "dummy_pcr_init";
    load_pcr(tc,pcr_file_path, pcr_init_func); 
    tc->pcr->mod(tc->pcr, mod);
    printf("PCR DUMMY loaded \n");

    tc->pcr->mod(tc->pcr, mod);
  } else if(mod->type == TC_PCR_5G_BDP){
//     int64_t now = time_now_us();
//     printf("ctrl elapsed time %ld \n", now - mod->bdp.tstamp);
    
    const char* pcr_file_path = TC_SRC_DIR "/build/pcr/libbdp_pcr.so";
    const char* pcr_init_func = "bdp_pcr_init";
    load_pcr(tc,pcr_file_path, pcr_init_func); 
    printf("PCR BDP loaded \n");

//   // tc->pcr->mod(tc->pcr, mod);
  } else {
    assert(0!=0 && "Unknwon type");
  }

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

tc_rc_t tc_del_pcr(tc_t* tc, tc_del_ctrl_pcr_t const* del)
{
  assert(tc != NULL);
  assert(del != NULL);

  assert(0!=0 && "Not implemented");
  return (tc_rc_t){.has_value = true, .tc = tc}; 
}



////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////

void tc_load_defaults(tc_t* tc)
{
  assert(tc != NULL);

  //load default scheduler 
  const char* sch_file_path = TC_SRC_DIR "/build/sch/librr_sch.so"; 
  const char* sch_init_func = "rr_init";
  load_sch(tc, sch_file_path, sch_init_func);

  //load default queue
  const char* queue_file_path = TC_SRC_DIR "/build/queue/libfifo_queue.so" ; 
  const char* queue_init_func = "fifo_init";
//  const char* queue_file_path = "/home/tiwa/mir/oai-tc/openair2/tc/queue/build/libcodel_queue.so"; 
//  const char* queue_init_func = "codel_init";
 
  queue_t* q = load_queue(tc, queue_file_path, queue_init_func);

  // create policer with NULL value
  const uint32_t drop_rate_kbps = 1000000;
  const uint32_t dev_rate_kbps =  2000000;
  const uint32_t time_window_us = 100000; 

  plc_t* plc = plc_init(drop_rate_kbps, dev_rate_kbps, time_window_us, q, q);

  // add queue to the scheduler
  tc->sch->add_queue(tc->sch, &q);

  printf("Queue added \n");
  fflush(stdout);

  // create the shaper
  shp_t* shp = shp_init(time_window_us, drop_rate_kbps); 
  printf("SHAPER added \n");

  // create a Round-Robin classifier
//  const char* cls_file_path = "/home/tiwa/mir/oai-tc/openair2/tc/cls/build/librr_cls.so"; 
//  const char* cls_init_func = "rr_cls_init";

  // Stocastic classifier
//  const char* cls_file_path = "/home/tiwa/mir/oai-tc/openair2/tc/cls/build/libsto_cls.so"; 
//  const char* cls_init_func = "sto_cls_init";

  // OSI classifier
  const char* cls_file_path = TC_SRC_DIR "/build/cls/libosi_cls.so"; 
  const char* cls_init_func = "osi_cls_init";

  load_cls(tc,q,cls_file_path, cls_init_func);
  printf("CLS added \n");

  // create a dummy pacer
//  const char* pcr_file_path = "/home/tiwa/mir/oai-tc/openair2/tc/pcr/build/libbdp_pcr.so";
//  const char* pcr_init_func = "bdp_pcr_init";
//  load_pcr(tc,pcr_file_path, pcr_init_func); 

  const char* pcr_file_path = TC_SRC_DIR "/build/pcr/libdummy_pcr.so";
  const char* pcr_init_func = "dummy_pcr_init";
  load_pcr(tc, pcr_file_path, pcr_init_func); 

  printf("PCR added \n");

  // init the hastable that maps Queues with Policers and Shapers
  const size_t key_sz = sizeof(uintptr_t);
  assoc_init(&tc->htab, key_sz, cmp_func_ht, free_func_ht);

  map_q_plc_shp(&tc->htab, &q, &plc, &shp); 
}

// Classifier management
tc_rc_t tc_conf_cls(tc_t* tc, tc_ctrl_cls_t const* cls)
{
  assert(tc != NULL);
  assert(cls != NULL);

  if(cls->act == TC_CTRL_ACTION_SM_V0_ADD){
    tc_add_cls(tc, &cls->add);
  } else if(cls->act == TC_CTRL_ACTION_SM_V0_DEL){
    tc_del_cls(tc, &cls->del);
  } else if(cls->act == TC_CTRL_ACTION_SM_V0_MOD){
    tc_mod_cls(tc, &cls->mod);
  } else {
    assert(0!=0 && "Unknown action type");
  }



  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

// Policer Management
tc_rc_t tc_conf_plc(tc_t* tc, tc_ctrl_plc_t const* plc)
{
  assert(tc != NULL);
  assert(plc != NULL);

  if(plc->act == TC_CTRL_ACTION_SM_V0_ADD){
    tc_add_plc(tc, &plc->add);
  } else if(plc->act == TC_CTRL_ACTION_SM_V0_DEL){
    tc_del_plc(tc, &plc->del);
  } else if(plc->act == TC_CTRL_ACTION_SM_V0_MOD){
    tc_mod_plc(tc, &plc->mod);
  } else {
    assert(0!=0 && "Unknown action type");
  }

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

// Queue management 
tc_rc_t tc_conf_q(tc_t* tc, tc_ctrl_queue_t const* q)
{
  assert(tc != NULL);
  assert(q != NULL);

  if(q->act == TC_CTRL_ACTION_SM_V0_ADD){
    tc_add_q(tc, &q->add);
  } else if(q->act == TC_CTRL_ACTION_SM_V0_DEL){
    tc_del_q(tc, &q->del);
  } else if(q->act == TC_CTRL_ACTION_SM_V0_MOD){
    tc_mod_q(tc, &q->mod);
  } else {
    assert(0!=0 && "Unknown action type");
  }

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

// Scheduler management
tc_rc_t tc_conf_sch(tc_t* tc, tc_ctrl_sch_t const* sch)
{
  assert(tc != NULL);
  assert(sch != NULL);

  if(sch->act == TC_CTRL_ACTION_SM_V0_ADD){
    tc_add_sch(tc, &sch->add);
  } else if(sch->act == TC_CTRL_ACTION_SM_V0_DEL){
    tc_del_sch(tc, &sch->del);
  } else if(sch->act == TC_CTRL_ACTION_SM_V0_MOD){
    tc_mod_sch(tc, &sch->mod);
  } else {
    assert(0!=0 && "Unknown action type");
  }

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

// Shaper management
tc_rc_t tc_conf_shp(tc_t* tc, tc_ctrl_shp_t const* shp)
{
  assert(tc != NULL);
  assert(shp != NULL);

  if(shp->act == TC_CTRL_ACTION_SM_V0_ADD){
    tc_add_shp(tc, &shp->add);
  } else if(shp->act == TC_CTRL_ACTION_SM_V0_DEL){
    tc_del_shp(tc, &shp->del);
  } else if(shp->act == TC_CTRL_ACTION_SM_V0_MOD){
    tc_mod_shp(tc, &shp->mod);
  } else {
    assert(0!=0 && "Unknown action type");
  }

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

// Pacer management
tc_rc_t tc_conf_pcr(tc_t* tc, tc_ctrl_pcr_t const* pcr)
{
  assert(tc != NULL);
  assert(pcr != NULL);

  if(pcr->act == TC_CTRL_ACTION_SM_V0_ADD){
    tc_add_pcr(tc, &pcr->add);
  } else if(pcr->act == TC_CTRL_ACTION_SM_V0_DEL){
    tc_del_pcr(tc, &pcr->del);
  } else if(pcr->act == TC_CTRL_ACTION_SM_V0_MOD){
    tc_mod_pcr(tc, &pcr->mod);
  } else {
    assert(0!=0 && "Unknown action type");
  }

  return (tc_rc_t){.has_value = true, .tc = tc}; 
}

