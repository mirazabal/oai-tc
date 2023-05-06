#include "tc_api.h"
#include "alg_ds/alg/alg.h"
#include "alg_ds/alg/comparisons.h"
#include "alg_ds/ds/lock_guard/lock_guard.h"

#include "time/time.h"
#include "tc.h"
#include "pkt.h"
#include "plc_shp_pair.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>

#ifndef TEST_TC_RF
#include "../LAYER2/nr_rlc/nr_rlc_oai_api.h"
#endif

// All the tc entities are saved in a binary tree.
static
//assoc_rb_tree_lck_t tc_entities;
assoc_rb_tree_t tc_entities;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static
pthread_t egress_thrd; 

static 
pthread_once_t init_once_tc_ds = PTHREAD_ONCE_INIT;

static 
atomic_bool b_stop_flag = false;


static
int create_timer(void)
{
  // Create the timer
  const int clockid = CLOCK_MONOTONIC;
  const int flags = TFD_NONBLOCK | TFD_CLOEXEC;
  const int tfd = timerfd_create(clockid, flags);
  assert(tfd != -1);

  const int flags_2 = 0;
  struct itimerspec *old_value = NULL; // not interested in how the timer was previously configured
  const struct timespec it_interval = {.tv_sec = 0, .tv_nsec = 200000};  /* Interval for periodic timer */
  const struct timespec it_value = {.tv_sec = 0, .tv_nsec = 200000};     /* Initial expiration */
  const struct itimerspec new_value = {.it_interval = it_interval, .it_value = it_value}; 
  int rc = timerfd_settime(tfd, flags_2, &new_value, old_value);
  assert(rc != -1);

  return tfd;
}

static
int create_epoll(int tfd)
{
  // Create epoll
  const int flags_3 = EPOLL_CLOEXEC; 
  const int efd = epoll_create1(flags_3);  
  assert(efd != -1);

  const int op = EPOLL_CTL_ADD;

  const epoll_data_t e_data = {.fd = tfd};
  const int e_events = EPOLLIN ; // open for reading
  struct epoll_event event = {.events = e_events, .data = e_data};
  int rc = epoll_ctl(efd, op, tfd, &event);
  assert(rc != -1);
  return efd;
}

static
void* tc_egress_task(void* arg)
{
  const int tfd = create_timer();
  const int efd = create_epoll(tfd);

  const int maxevents = 32;
  struct epoll_event events[maxevents];
  const int timeout_ms = 1000;

  int count = 0; 
  while(b_stop_flag == false){
    const int events_ready = epoll_wait(efd, events, maxevents, timeout_ms); 
    if(events_ready == -1){
      printf("Error at epoll_wait = %s \n", strerror(errno));
    }
    assert(events_ready != -1);

    for(int i =0; i < events_ready; ++i){
      assert((events[i].events & EPOLLERR) == 0);
      const int cur_fd = events[i].data.fd; 
      if (cur_fd == tfd){
        uint64_t res;
        ssize_t r = read(tfd, &res, sizeof(uint64_t));
        assert(r != 0);
        //printf("Timer expired %lu times!\n", res);
      }
    }

    void* it = NULL;
    void* last = NULL;
    {
      lock_guard(&mtx);
      it = assoc_front(&tc_entities);
      last = assoc_end(&tc_entities);
    }

    ++count;
    while(it != last){
      tc_t* tc = NULL;
      {
        lock_guard(&mtx);
        tc = assoc_value(&tc_entities, it);
      }
      tc_egress_pkts(tc);
      {
        lock_guard(&mtx);
        it = assoc_next(&tc_entities, it);
      }

#ifndef TEST_TC_RF
      if(count % 5 == 0 && tc->rb_id != 0){
        nr_rlc_entity_buffer_status_t b = nr_rlc_get_buffer_status(tc->rnti, tc->rb_id);
        tc_rc_t r = tc_drb_size(tc, b.tx_size);
        (void)r;
      }
#endif
    }
  
  }

  int rc = close(efd);
  assert(rc != -1);

  rc = close(tfd); 
  assert(rc != -1);

  return NULL;
}

static inline
uint64_t generate_key(uint32_t rnti, uint32_t rb_id)
{
  uint64_t key = (uint64_t)(rnti) << 32 | (uint64_t)(rb_id);
  return key;
}

static inline
void free_tc_entity(void* key, void* value)
{
  assert(key != NULL);
  assert(value != NULL);

  tc_free(value);
}

static
void tc_stop(void)
{
  assert(b_stop_flag == false);
  b_stop_flag = true;

  int const rc = pthread_join(egress_thrd, NULL);
  assert(rc == 0);

  assoc_free(&tc_entities);
}


static
char stdout_buf[65536];


static
void init_tc_api(void)
{
  assoc_init(&tc_entities, sizeof(uint64_t), cmp_uint64_t, free_tc_entity);

  int rc = pthread_create(&egress_thrd, NULL, tc_egress_task, NULL);
  assert(rc == 0);

  rc = atexit(tc_stop);
  assert(rc == 0);

  rc = setvbuf(stdout, stdout_buf, _IOFBF, 65536); // 1024);
  assert(rc == 0);

}

static inline
bool tc_found(void const* it, assoc_rb_tree_t const* tc_entities )
{
   return it != assoc_end(tc_entities);
}

tc_rc_t tc_get_or_create(uint32_t rnti, uint32_t rb_id)
{
  assert(rnti != 0 && "Most probably a bug even though probably permitted by 3gpp");
  assert(rb_id < 30);

  int rc = pthread_once(&init_once_tc_ds, init_tc_api);
  assert(rc == 0);

  uint64_t const key = generate_key(rnti, rb_id);
  void* it = NULL; 
  {
    lock_guard(&mtx);
    it = find_if_r(&tc_entities, &key, eq_uint64_t);
  }


  if(tc_found(it, &tc_entities) == false){
    tc_t* tc = calloc(1, sizeof(tc_t));
    assert(tc != NULL);

    tc_init(tc, rb_id, rnti);
    tc_load_defaults(tc);

    {
      lock_guard(&mtx);
      it = assoc_insert(&tc_entities, &key, sizeof(key), tc);
    }

    assert(it != NULL);
    assert(it != assoc_end(&tc_entities));
    assert(assoc_value(&tc_entities, it) == tc);
  } 

  tc_t* tc = assoc_value(&tc_entities, it);

  assert(tc != NULL);
  assert(tc->rnti == rnti);
  assert(tc->rb_id == rb_id);

  return (tc_rc_t){.tc = tc, .has_value = true};
}

// Ingress data (DL)
tc_rc_t tc_data_req(tc_handle_t* tc_h, uint8_t* data, size_t sz)
{
  assert(tc_h != NULL);
  assert(data != NULL);
  assert(sz < MTU_SIZE + 1 && sz != 0); 

  tc_t* tc = (tc_t*)tc_h;

  tc_ingress_pkts(tc, data, sz);

  return (tc_rc_t){.has_value = true, .tc = tc};
}

// Egress data (DL)
tc_rc_t tc_data_ind(tc_handle_t* tc_h, void (*egress_fun)(uint16_t rnti, uint8_t rb_id, uint8_t* data, size_t sz) )
{
  assert(tc_h != NULL);
  assert(egress_fun != NULL);

  tc_t* tc = (tc_t*)tc_h;
  tc->egress_fun = egress_fun;
  tc_rc_t rc = {.has_value = true, .tc = tc };
  return rc;
}

// Refresh the DRB size in bytes
tc_rc_t tc_drb_size(tc_handle_t* tc_h, size_t sz)
{
  assert(tc_h != NULL);
  tc_t* tc = (tc_t*)tc_h; 

  lock_guard(&tc->mtx);

  assert(tc->pcr != NULL);
  pcr_t* p = tc->pcr;

  if(p->type == TC_PCR_5G_BDP){
    tc_ctrl_pcr_t ctrl = { 0 }; 
    ctrl.act = TC_CTRL_ACTION_SM_V0_MOD;
    ctrl.mod.bdp.drb_sz = sz;
    ctrl.mod.bdp.tstamp = time_now_us();
    p->mod(p, &ctrl.mod);
  }


  //printf("TC DRB size %ld tstamp %ld \n", sz, time_now_us());

//  TC_CTRL_ACTION_SM_V0_ADD,
//  TC_CTRL_ACTION_SM_V0_DEL,
//  if(p->type == TC_PCR_5G_BDP){
//    ctrl.bdp.drb_sz = sz;
//    ctrl.bdp.tstamp= time_now_us();
//  }

  tc_rc_t rc = {.has_value = true, .tc = tc};
  return rc;
}


// Statistics from the tc
tc_ind_data_t tc_ind_data(tc_handle_t const* tc_h)
{
  assert(tc_h != NULL);
  
  tc_t* tc = (tc_t*)tc_h;

  tc_ind_data_t ind = {0};

  tc_ind_msg_t* msg = &ind.msg;

  msg->tstamp = time_now_us();

  lock_guard(&tc->mtx);

  // Sched stats
  msg->sch = tc->sch->stats(tc->sch);

  // Pacer stats
  msg->pcr = tc->pcr->stats(tc->pcr);

  assert(seq_size(&tc->clss ) > 0);
  cls_t* cls = *(cls_t**)seq_front(&tc->clss);
 
  // Classifier stats
  msg->cls = cls->stats(cls);

  msg->len_q = seq_size(&tc->queues); 

  if(msg->len_q > 0){
    msg->q = calloc(msg->len_q, sizeof(tc_queue_t));
    assert(msg->q != NULL && "Memory exhausted");

    msg->shp = calloc(msg->len_q, sizeof(tc_shp_t));
    assert(msg->shp != NULL && "Memory exhausted");

    msg->plc = calloc(msg->len_q, sizeof(tc_plc_t));
    assert(msg->plc != NULL && "Memory exhausted");
  }

  for(size_t i = 0; i < msg->len_q; ++i){
    queue_t* q = *(queue_t** )seq_at(&tc->queues, i); 

    msg->q[i] = q->stats(q); 

    plc_shp_pair_t* p = find_plc_shp(&tc->htab , &q);

    msg->shp[i] = shp_stat(p->shp); // get_shp_stats(tc , q);
    msg->shp[i].id = q->id;

    msg->plc[i] = plc_stat(p->plc); //get_plc_stats(tc , q); 
  }

  return ind;
}


/*

//////////
/// ADD
//////////

static
void tc_conf_add_cls(tc_handle_t* tc, tc_ctrl_cls_t const* cls )
{
  assert(cls != NULL);

}

static
void tc_conf_add_plc(tc_handle_t* tc, tc_ctrl_plc_t const* plc)
{
  assert(plc != NULL);

}


static 
void tc_conf_add_queue(tc_handle_t* tc, tc_add_ctrl_queue_t const* q)
{
  assert(q != NULL);

  if(q->type == TC_QUEUE_CODEL) {
    const char* file_path = "/home/mir/workspace/tc/queue/build/libcodel_queue.so"; 
    const char* init_func = "codel_init";
    tc_add_q(tc, file_path, init_func);
  } else if(q->type == TC_QUEUE_FIFO) {
    const char* file_path = "/home/mir/workspace/tc/queue/build/libfifo_queue.so"; 
    const char* init_func = "fifo_init";
    tc_add_q(tc, file_path, init_func);
  } else {
    assert(0!=0 && "Unknwon queu type");
  }

}


static
void tc_conf_add_sch(tc_handle_t* tc, tc_ctrl_sch_t const* sch)
{
  assert(sch != NULL);

}


static 
void tc_conf_add_shp(tc_handle_t* tc,tc_ctrl_shp_t const* shp)
{
  assert(shp != NULL);

}


static 
void tc_conf_add_pcr(tc_handle_t* tc, tc_ctrl_pcr_t const* pcr)
{
  assert(pcr != NULL);


}

static
void tc_conf_add(tc_handle_t* tc, tc_ctrl_msg_t* msg)
{ 
  assert(tc != NULL);
  assert(msg != NULL);

  assert(msg->type == TC_CTRL_SM_V0_QUEUE && "For testting purposes");

  if(msg->type == TC_CTRL_SM_V0_CLS ){
    tc_conf_add_cls(tc, &msg->cls);
  } else if (msg->type == TC_CTRL_SM_V0_PLC ){
    tc_conf_add_plc(tc, &msg->plc);
  } else if (msg->type == TC_CTRL_SM_V0_QUEUE ){
    tc_conf_add_queue(tc, &msg->q.add);
  } else if (msg->type == TC_CTRL_SM_V0_SCH){
    tc_conf_add_sch(tc, &msg->sch);
  } else if (msg->type == TC_CTRL_SM_V0_SHP){
    tc_conf_add_shp(tc, &msg->shp);
  } else if (msg->type == TC_CTRL_SM_V0_PCR){
    tc_conf_add_pcr(tc, &msg->pcr);
  } else {
    assert(0 != 0 && "Unknown message type");
  }

}


//////////
/// MOD
//////////

static
void tc_conf_mod_cls(tc_handle_t* tc, tc_ctrl_cls_t const* cls )
{
  assert(tc != NULL);
  assert(cls != NULL);
  assert(cls->act = TC_CTRL_ACTION_SM_V0_MOD);

}

static
void tc_conf_mod_plc(tc_handle_t* tc, tc_ctrl_plc_t const* plc)
{
  assert(tc != NULL);
  assert(plc != NULL);
  assert(plc->act = TC_CTRL_ACTION_SM_V0_MOD);

  tc_mod_plc(tc, &plc->mod);
}


static 
void tc_conf_mod_queue(tc_handle_t* tc, tc_ctrl_queue_t const* q)
{
  assert(tc != NULL);
  assert(q != NULL);
  assert(q->act = TC_CTRL_ACTION_SM_V0_MOD);

  tc_mod_q(tc, &q->mod);

}


static
void tc_conf_mod_sch(tc_handle_t* tc, tc_ctrl_sch_t const* sch)
{
  assert(tc != NULL);
  assert(sch != NULL);
  assert(sch->act == TC_CTRL_ACTION_SM_V0_MOD);

}


static 
void tc_conf_mod_shp(tc_handle_t* tc,tc_ctrl_shp_t const* shp)
{
  assert(tc != NULL);
  assert(shp != NULL);

  tc_mod_shp(tc, &shp->mod );
}


static 
void tc_conf_mod_pcr(tc_handle_t* tc, tc_ctrl_pcr_t const* pcr)
{
  assert(tc != NULL);
  assert(pcr != NULL);

  tc_mod_pcr(tc, pcr);

}


static
void tc_conf_mod(tc_handle_t* tc, tc_ctrl_msg_t* msg)
{
  assert(tc != NULL);
  assert(msg != NULL);

  if(msg->type == TC_CTRL_SM_V0_CLS ){
    tc_conf_mod_cls(tc, &msg->cls );
  } else if(msg->type == TC_CTRL_SM_V0_PLC){
    tc_conf_mod_plc(tc, &msg->plc );
  } else if(msg->type == TC_CTRL_SM_V0_QUEUE ){
    tc_conf_mod_queue(tc, &msg->q);
  } else if(msg->type == TC_CTRL_SM_V0_SCH ){
    tc_conf_mod_sch(tc, &msg->sch);
  } else if(msg->type == TC_CTRL_SM_V0_SHP ){
    tc_conf_mod_shp(tc, &msg->shp );
  } else if(msg->type == TC_CTRL_SM_V0_PCR){
    tc_conf_mod_pcr(tc, &msg->pcr );
  }


} 


//////////
/// DEL
//////////

static
void tc_conf_del_cls(tc_handle_t* tc, tc_ctrl_cls_t const* cls )
{
  assert(cls != NULL);

}

static
void tc_conf_del_plc(tc_handle_t* tc, tc_ctrl_plc_t const* plc)
{
  assert(plc != NULL);

}


static 
void tc_conf_del_queue(tc_handle_t* tc, tc_ctrl_queue_t const* q)
{
  assert(tc != NULL);
  assert(q != NULL);
  tc_del_q(tc, q->del.id, q->del.type);
}


static
void tc_conf_del_sch(tc_handle_t* tc, tc_ctrl_sch_t const* sch)
{
  assert(sch != NULL);

}


static 
void tc_conf_del_shp(tc_handle_t* tc,tc_ctrl_shp_t const* shp)
{
  assert(shp != NULL);

}


static 
void tc_conf_del_pcr(tc_handle_t* tc, tc_ctrl_pcr_t const* pcr)
{
  assert(pcr != NULL);

}


static
void tc_conf_del(tc_handle_t* tc, tc_ctrl_msg_t* msg)
{
  assert(tc != NULL);
  assert(msg != NULL);

  if(msg->type == TC_CTRL_SM_V0_CLS ){
    tc_conf_del_cls(tc, &msg->cls);
  } else if (msg->type == TC_CTRL_SM_V0_PLC ){
    tc_conf_del_plc(tc, &msg->plc);
  } else if (msg->type == TC_CTRL_SM_V0_QUEUE ){
    tc_conf_del_queue(tc, &msg->q);
  } else if (msg->type == TC_CTRL_SM_V0_SCH){
    tc_conf_del_sch(tc, &msg->sch);
  } else if (msg->type == TC_CTRL_SM_V0_SHP){
    tc_conf_del_shp(tc, &msg->shp);
  } else if (msg->type == TC_CTRL_SM_V0_PCR){
    tc_conf_del_pcr(tc, &msg->pcr);
  } else {
    assert(0 != 0 && "Unknown message type");
  }

}

*/

tc_ctrl_out_t tc_conf(tc_handle_t* tc, tc_ctrl_msg_t const* msg)
{
  assert(tc != NULL);
  assert(msg != NULL);

  if(msg->type == TC_CTRL_SM_V0_CLS){
    tc_conf_cls(tc, &msg->cls);
  } else if(msg->type == TC_CTRL_SM_V0_PLC){
    tc_conf_plc(tc, &msg->plc);
  } else if(msg->type == TC_CTRL_SM_V0_QUEUE){
    tc_conf_q(tc, &msg->q);
  } else if(msg->type == TC_CTRL_SM_V0_SCH){
    tc_conf_sch(tc, &msg->sch);
  } else if(msg->type == TC_CTRL_SM_V0_SHP){
    tc_conf_shp(tc, &msg->shp);
  } else if(msg->type == TC_CTRL_SM_V0_PCR){
    tc_conf_pcr(tc, &msg->pcr);
  } else {
    assert(0!=0 && "Unknown message type");
  }

/*
  if(msg->act == TC_CTRL_ACTION_SM_V0_ADD ){
    tc_conf_add(tc, msg);
  } else if(msg->act == TC_CTRL_ACTION_SM_V0_DEL){
    tc_conf_del(tc, msg);
  } else if(msg->act == TC_CTRL_ACTION_SM_V0_MOD){
    tc_conf_mod(tc, msg);
  } else {
    assert(0!=0 && "Unknwon action type" ); 
  }
*/
  tc_ctrl_out_t ans = {.out =  TC_CTRL_OUT_OK};
  return ans;
}


