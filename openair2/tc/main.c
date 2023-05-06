#include "tc_api.h"
#include "tc.h"

#include "time/time.h"

#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <netinet/ip.h>
#include <poll.h>

pthread_t t_ing;

pthread_t t_rlc;

pthread_t t_e2;

static
const uint32_t rnti = 100;

static
const uint32_t rb_id = 4;

static
atomic_bool stop_ing = false; 

static
void* ingress_func(void* arg)
{
  time_t t;
  srand((unsigned) time(&t));

  for(int i = 0; i < 100000 && stop_ing == false; ++i){
    tc_rc_t rc = tc_get_or_create(rnti, rb_id);
    if(!rc.has_value)
      assert(0 != 0 && "Error getting the tc");

    uint8_t* data = malloc(sizeof(uint8_t)* 1500);
    struct iphdr* hdr = (void* ) data;

    srand(time(NULL));
    int const var = rand()%3;
    
    hdr->protocol = var == 0 ? IPPROTO_TCP : var == 1 ? IPPROTO_UDP : IPPROTO_ICMP; //  6; // TCP

    tc_data_req(rc.tc, data, 1500);

    uint32_t const time_sleep = rand() % 1000 + 10; 
    poll(NULL, 0, time_sleep);

    free(data);
  }

  printf("Ingress thread finished!");
  return NULL;
}

static 
atomic_bool run_rlc_drb = true;

static
void* rlc_drb(void* arg)
{
  time_t t;
  srand((unsigned) time(&t));

  while(run_rlc_drb){
    usleep(1000);
    tc_rc_t rc = tc_get_or_create(rnti, rb_id);
    if(!rc.has_value)
      continue;

    uint32_t const rand_num = rand() % 2200;

    tc_drb_size(rc.tc, rand_num);
  }
  return NULL;
}

void egress_fun(uint16_t rnti, uint8_t rb_id, uint8_t* data, size_t sz) 
{
  assert(data != NULL);
  assert(sz > 0);
  printf("Egressing pkt \n");

  //free(data);
}

static
atomic_bool run_e2 = true;

static 
void* read_RAN(void* d)
{
  while(run_e2){
    usleep(1000);
    tc_rc_t rc = tc_get_or_create(rnti, rb_id);
    if(!rc.has_value)
      continue;
    assert(rc.tc != NULL);

    tc_ind_data_t ind = tc_ind_data(rc.tc);

    printf("Indication data tstamp = %ld and num queues = %u \n", ind.msg.tstamp, ind.msg.len_q);

    for(size_t i = 0; i < ind.msg.len_q; ++i){
       tc_queue_t* q = &ind.msg.q[i];
      if(q->type == TC_QUEUE_FIFO){
        printf("FIFO bytes = %u , pkt = %u ", q->fifo.bytes, q->fifo.pkts );
      } else if(q->type == TC_QUEUE_CODEL){
        printf("CODEL bytes = %u , pkt = %u ", q->codel.bytes, q->codel.pkts );
      } else {
        assert(0!=0 && "Unknown type");
      } 

      printf("Queue Id = %d \n" ,ind.msg.q[i].id);
    }

    free_tc_ind_data(&ind); 
  }

  return NULL;
}

static
tc_ctrl_msg_t gen_add_codel_queue(void)
{

  tc_ctrl_msg_t ans = {.type = TC_CTRL_SM_V0_QUEUE,
                       .q.act = TC_CTRL_ACTION_SM_V0_ADD}; 

  tc_add_ctrl_queue_t* q = &ans.q.add;

  q->type = TC_QUEUE_CODEL;

  q->codel.interval_ms = 100;
  q->codel.target_ms = 5;

  assert(ans.type ==  TC_CTRL_SM_V0_QUEUE );

  return ans;
}

static
tc_ctrl_msg_t gen_del_queue(uint32_t id, tc_queue_e type)
{
  assert(id < 16 && "Do you have more than 16 queues ?");

  tc_ctrl_msg_t ans = {.type = TC_CTRL_SM_V0_QUEUE,
                       .q.act = TC_CTRL_ACTION_SM_V0_DEL}; 

  ans.q.del.id = id;
  ans.q.del.type = type; 

  return ans;
}

static
tc_ctrl_msg_t gen_mod_codel_queue(uint32_t id_4, uint32_t interval_ms, uint32_t target_ms)
{
  tc_ctrl_msg_t ans = {.type = TC_CTRL_SM_V0_QUEUE,
                        .q.act = TC_CTRL_ACTION_SM_V0_MOD}; 

  ans.q.mod.id = id_4;
  ans.q.mod.type = TC_QUEUE_CODEL;
  ans.q.mod.codel.interval_ms = interval_ms;
  ans.q.mod.codel.target_ms = target_ms;

  return ans;
}

static
tc_ctrl_msg_t gen_mod_shp(uint32_t id_4, uint32_t max_rate_kbps, uint32_t time_window_ms)
{
  tc_ctrl_msg_t ans = {.type = TC_CTRL_SM_V0_SHP, 
                        .shp.act = TC_CTRL_ACTION_SM_V0_MOD}; 

  ans.shp.mod.active = 1;
  ans.shp.mod.id = id_4;
  ans.shp.mod.max_rate_kbps = max_rate_kbps;
  ans.shp.mod.time_window_ms = time_window_ms;

  return ans;
}

static
tc_ctrl_msg_t gen_mod_plc(uint32_t id, uint32_t dev_id, uint32_t dev_rate_kbps, uint32_t drop_rate_kbps )
{
  tc_ctrl_msg_t ans = {.type = TC_CTRL_SM_V0_PLC, 
                        .plc.act = TC_CTRL_ACTION_SM_V0_MOD}; 

  ans.plc.mod.active = 1;
  ans.plc.mod.id = id; 
  ans.plc.mod.dev_id = dev_id;
  ans.plc.mod.dev_rate_kbps = dev_rate_kbps ;
  ans.plc.mod.drop_rate_kbps = drop_rate_kbps ;

  return ans;
}

static
tc_ctrl_msg_t gen_mod_bdp_pcr(void)
{
  tc_ctrl_msg_t ans = {.type = TC_CTRL_SM_V0_PCR, 
                        .pcr.act = TC_CTRL_ACTION_SM_V0_MOD}; 

  ans.pcr.mod.type = TC_PCR_5G_BDP;

  ans.pcr.mod.bdp.tstamp = time_now_us();
  ans.pcr.mod.bdp.drb_sz = 1500;

  return ans;
}

static
tc_ctrl_msg_t gen_mod_dummy_pcr(void)
{
  tc_ctrl_msg_t ans = {.type = TC_CTRL_SM_V0_PCR, 
                        .pcr.act = TC_CTRL_ACTION_SM_V0_MOD}; 

  ans.pcr.mod.type = TC_PCR_DUMMY;

  return ans;
}

tc_ctrl_msg_t gen_add_osi_cls()
{
  tc_ctrl_msg_t ans = {.type = TC_CTRL_SM_V0_CLS, 
                        .cls.act = TC_CTRL_ACTION_SM_V0_ADD}; 

  tc_add_ctrl_cls_t* add = &ans.cls.add; 
  add->type = TC_CLS_OSI;
  add->osi.dst_queue = 4;
  add->osi.l3.src_addr = -1;
  add->osi.l3.dst_addr = -1;
  add->osi.l4.src_port = -1;
  add->osi.l4.dst_port = -1;
  add->osi.l4.protocol = IPPROTO_TCP;

  return ans;
}

tc_ctrl_msg_t gen_add_osi_cls_2()
{
  tc_ctrl_msg_t ans = {.type = TC_CTRL_SM_V0_CLS, 
                        .cls.act = TC_CTRL_ACTION_SM_V0_ADD}; 

  tc_add_ctrl_cls_t* add = &ans.cls.add; 
  add->type = TC_CLS_OSI;
  add->osi.dst_queue = 5;
  add->osi.l3.src_addr = -1;
  add->osi.l3.dst_addr = -1;
  add->osi.l4.src_port = -1;
  add->osi.l4.dst_port = -1;
  add->osi.l4.protocol = IPPROTO_UDP;

  return ans;
}


tc_ctrl_msg_t gen_mod_osi_cls()
{
  tc_ctrl_msg_t ans = {.type = TC_CTRL_SM_V0_CLS, 
                        .cls.act = TC_CTRL_ACTION_SM_V0_MOD}; 

  tc_mod_ctrl_cls_t* mod = &ans.cls.mod; 
  mod->type = TC_CLS_OSI;
  mod->osi.filter.dst_queue = 0;
  mod->osi.filter.id = 0;
  mod->osi.filter.l3.src_addr = -1;
  mod->osi.filter.l3.dst_addr = -1;
  mod->osi.filter.l4.src_port = -1;
  mod->osi.filter.l4.dst_port = -1;
  mod->osi.filter.l4.protocol = IPPROTO_TCP;

  return ans;
}

int main()
{
  tc_rc_t rc = tc_get_or_create(rnti, rb_id);
  if(!rc.has_value)
    assert(0 != 0 && "Error getting the tc");

  rc = tc_data_ind(rc.tc, egress_fun);

  void *ret;
  if (pthread_create(&t_ing, NULL, ingress_func, NULL) != 0) {
    exit(1);
  }

  if(pthread_create(&t_rlc, NULL, rlc_drb, NULL) != 0) {
    exit(1);
  }

  if(pthread_create(&t_e2, NULL, read_RAN, NULL) != 0) {
    exit(1);
  }

  tc_ctrl_msg_t co_q = gen_add_codel_queue();
  assert(co_q.type == TC_CTRL_SM_V0_QUEUE);
  assert(co_q.q.act == TC_CTRL_ACTION_SM_V0_ADD);

  tc_ctrl_out_t ans = tc_conf(rc.tc, &co_q);
  assert(ans.out == TC_CTRL_OUT_OK );

  ans = tc_conf(rc.tc, &co_q);
  assert(ans.out == TC_CTRL_OUT_OK );

  ans = tc_conf(rc.tc, &co_q);
  assert(ans.out == TC_CTRL_OUT_OK );

  ans = tc_conf(rc.tc, &co_q);
  assert(ans.out == TC_CTRL_OUT_OK );

  ans = tc_conf(rc.tc, &co_q);
  assert(ans.out == TC_CTRL_OUT_OK );

  sleep(1);

  uint32_t const id_2 = 2;
  tc_queue_e del_type = TC_QUEUE_CODEL;
  tc_ctrl_msg_t del_q = gen_del_queue(id_2, del_type);

  ans = tc_conf(rc.tc, &del_q);
  assert(ans.out == TC_CTRL_OUT_OK );
  printf("Delete queue sent\n");

  sleep(1);

  uint32_t const id_3 = 3;
  del_q = gen_del_queue(id_3, del_type);

  ans = tc_conf(rc.tc, &del_q);
  assert(ans.out == TC_CTRL_OUT_OK );
  printf("Delete queue sent\n");

  uint32_t const id_4 = 4;
  uint32_t interval_ms = 300;
  uint32_t target_ms = 15;

  tc_ctrl_msg_t codel_param = gen_mod_codel_queue(id_4, interval_ms, target_ms);

  ans = tc_conf(rc.tc, &codel_param );
  assert(ans.out == TC_CTRL_OUT_OK );
  printf("Modify Codel queue sent\n");


  uint32_t time_window_ms = 100;
  uint32_t max_rate_kbps = 10000; // 10 Mbits
  tc_ctrl_msg_t mod_shp = gen_mod_shp(id_4, max_rate_kbps, time_window_ms);

  ans = tc_conf(rc.tc, &mod_shp);
  assert(ans.out == TC_CTRL_OUT_OK );
  printf("Modify shaper conf sent\n");

  uint32_t dev_id = 5;
  uint32_t dev_rate_kbps = 10000;
  uint32_t drop_rate_kbps = 15000;
  tc_ctrl_msg_t mod_plc = gen_mod_plc(id_4, dev_id, dev_rate_kbps, drop_rate_kbps );

  ans = tc_conf(rc.tc, &mod_plc);
  assert(ans.out == TC_CTRL_OUT_OK );
  printf("Modify policer conf sent\n");

  sleep(1);

  tc_ctrl_msg_t mod_dummy_pcr = gen_mod_dummy_pcr();
  ans = tc_conf(rc.tc, &mod_dummy_pcr);
  assert(ans.out == TC_CTRL_OUT_OK );
  printf("Modify dummy pacer conf sent\n");

  sleep(1);

  tc_ctrl_msg_t mod_pcr = gen_mod_bdp_pcr();
  ans = tc_conf(rc.tc, &mod_pcr);
  assert(ans.out == TC_CTRL_OUT_OK );
  printf("Modify pacer conf sent\n");

  sleep(1);

  tc_ctrl_msg_t add_osi_cls = gen_add_osi_cls();
  ans = tc_conf(rc.tc, &add_osi_cls);
  assert(ans.out == TC_CTRL_OUT_OK );

  tc_ctrl_msg_t add_osi_cls_2 = gen_add_osi_cls_2();
  ans = tc_conf(rc.tc, &add_osi_cls_2);
  assert(ans.out == TC_CTRL_OUT_OK );
  printf("ADD cls OSI conf sent\n");

  sleep(1);
  
  tc_ctrl_msg_t mod_osi_cls = gen_mod_osi_cls();
  ans = tc_conf(rc.tc, &mod_osi_cls);
  assert(ans.out == TC_CTRL_OUT_OK );
  printf("Modify cls OSI conf sent\n");

/*
  sleep(1);
  tc_ctrl_msg_t mod_rr_cls = gen_mod_rr_cls();
  ans = tc_conf(rc.tc, &mod_rr_cls);
  assert(ans.out == TC_CTRL_OUT_OK );
  printf("Modify cls OSI conf sent\n");

  sleep(1);

  tc_ctrl_msg_t mod_sto_cls = gen_mod_sto_cls();
  ans = tc_conf(rc.tc, &mod_sto_cls);
  assert(ans.out == TC_CTRL_OUT_OK );
  printf("Modify cls OSI conf sent\n");

  sleep(1);
*/


  sleep(5);

  stop_ing = true;
  pthread_join(t_ing, &ret);

  run_rlc_drb = false; 
  pthread_join(t_rlc, &ret);

  run_e2 = false;
  pthread_join(t_e2, &ret);

  printf("Test run SUCCESSFULLY. At exit still running... \n" );

  return 0;
}

