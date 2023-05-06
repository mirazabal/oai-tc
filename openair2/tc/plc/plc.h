#ifndef PLC_H
#define PLC_H

#include "../queue/queue.h" 
#include "../alg_ds/ds/seq_container/seq_generic.h"
#include "../alg_ds/meter/meter.h"
#include <stdbool.h>
#include <stdint.h>


typedef struct plc_s
{
  mtr_t m;
  uint32_t drop_rate_kbps;  
  uint32_t dev_rate_kbps;  

  uint32_t active;
  uint32_t dropped_pkts;
  queue_t* dst_q;
  queue_t* dev_q;
} plc_t;

plc_t* plc_init(uint32_t drop_rate_kbps, uint32_t dev_rate_kbps, uint32_t time_window_us, queue_t* dst_q, queue_t* dev_q);  

void plc_free(plc_t*);

void plc_bytes_fwd(plc_t* plc, uint32_t bytes);

void plc_set_dev_q(plc_t* plc, queue_t* dev_q);


typedef enum plc_act{
    PLC_DROP,
    PLC_PASS,
    PLC_DEVIATE
  } plc_act_e;

plc_act_e plc_action(plc_t* plc, uint32_t bytes);

tc_plc_t plc_stat(plc_t* plc);

void plc_mod(plc_t* p, queue_t** dev_q, tc_mod_ctrl_plc_t const* ctrl);

/*
// Optional Policer type 
typedef struct opt_plc_s
{
  plc_t p;
  bool has_value;
} opt_plc_t;
*/

#endif
