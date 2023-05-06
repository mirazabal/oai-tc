#ifndef TC_H
#define TC_H

#include "tc_api.h"

#include "alg_ds/ds/seq_container/seq_generic.h"
#include "alg_ds/ds/assoc_container/assoc_generic.h"
#include "cls/cls.h"
#include "queue/queue.h"
#include "pcr/pcr.h"
#include "plc/plc.h"
#include "sch/sch.h"
#include "shp/shp.h"


#include <pthread.h>


typedef struct {
  const uint32_t rb_id;
  const uint32_t rnti;

  // Ingress
  seq_arr_t clss; // cls_t* Only one supported at the moment
  seq_arr_t queues; // queue_t* 
//  seq_arr_t plcs; // plc_t*

  // Egress
  sch_t* sch;
  pcr_t* pcr;
//  seq_arr_t shps;

  // All the queues (queue_t*) have associated a policer and a shapper
  assoc_ht_open_t htab;

  // Egress function
  void (*egress_fun)(uint16_t rnti, uint8_t rb_id, uint8_t*, size_t sz); 

  // Mutex for manipulating the internal tc ds
  pthread_mutex_t mtx;

} tc_t;



void tc_init(tc_t* tc, uint32_t rb_id, uint32_t rnti);

void tc_free(tc_t* tc);

void tc_load_defaults(tc_t* tc);

void tc_egress_pkts(tc_t* tc);

void tc_ingress_pkts(tc_t* tc, uint8_t* data, size_t sz);

// Classifier management
tc_rc_t tc_conf_cls(tc_t*, tc_ctrl_cls_t const* cls);

// Policer Management
tc_rc_t tc_conf_plc(tc_t*, tc_ctrl_plc_t const* plc);

// Queue management 
tc_rc_t tc_conf_q(tc_t*, tc_ctrl_queue_t const* q);

//tc_rc_t tc_add_q(tc_t* tc, const char* file_path, const char* init_func);

// Scheduler management
tc_rc_t tc_conf_sch(tc_t*, tc_ctrl_sch_t const* sch);

// Shaper management
tc_rc_t tc_conf_shp(tc_t*, tc_ctrl_shp_t const* shp);

// Pacer management
tc_rc_t tc_conf_pcr(tc_t*, tc_ctrl_pcr_t const* pcr);

#endif

