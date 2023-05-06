#ifndef TRAFFIC_CONTROL_API
#define TRAFFIC_CONTROL_API 

#define TC_SM

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tc_sm/ie/tc_data_ie.h"

typedef void tc_handle_t;

// Return code either with the handle or the reason why it failed
typedef struct {
 union {
    tc_handle_t* tc;
    char const* reason;
  };
  bool has_value; 
} tc_rc_t;


tc_rc_t tc_get_or_create(uint32_t rnti, uint32_t rb_id);

// Ingress data (DL)
tc_rc_t tc_data_req(tc_handle_t* tc, uint8_t* data, size_t sz);

// Egress data (DL)
tc_rc_t tc_data_ind(tc_handle_t* tc, void (*egress_fun)(uint16_t rnti, uint8_t rb_id, uint8_t* data, size_t sz) );

// Refresh the DRB size in bytes
tc_rc_t tc_drb_size(tc_handle_t* tc, size_t sz);

// Stop and free the Traffic Control module
// Called atexit
//void tc_stop(void);

/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////
/////////////////////////////////////////

// Statistics
tc_ind_data_t tc_ind_data(tc_handle_t const* tc_h);

// Configuration (add/delete)  
tc_ctrl_out_t tc_conf(tc_handle_t* tc, tc_ctrl_msg_t const* ctrl);

#endif

