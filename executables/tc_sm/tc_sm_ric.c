#include "tc_sm_ric.h"
#include "tc_sm_id.h"

#include <assert.h>
#include <stdlib.h>

#include "enc/tc_enc_generic.h"
#include "dec/tc_dec_generic.h"

typedef struct{
  sm_ric_t base;

#ifdef ASN
  tc_enc_asn_t enc;
#elif FLATBUFFERS 
  tc_enc_fb_t enc;
#elif PLAIN
  tc_enc_plain_t enc;
#else
  static_assert(false, "No encryption type selected");
#endif
} sm_tc_ric_t;


static
sm_subs_data_t on_subscription_tc_sm_ric(sm_ric_t const* sm_ric, const char* cmd)
{
  assert(sm_ric != NULL); 
  assert(cmd != NULL); 
  sm_tc_ric_t* sm = (sm_tc_ric_t*)sm_ric;  
 
  tc_event_trigger_t ev = {0};

  const int max_str_sz = 10;
  if(strncmp(cmd, "1_ms", max_str_sz) == 0 ){
    ev.ms = 1;
  } else if (strncmp(cmd, "2_ms", max_str_sz) == 0 ) {
    ev.ms = 2;
  } else if (strncmp(cmd, "5_ms", max_str_sz) == 0 ) {
    ev.ms = 5;
  } else {
    assert(0 != 0 && "Invalid input");
  }
  const byte_array_t ba = tc_enc_event_trigger(&sm->enc, &ev); 

  sm_subs_data_t data = {0}; 
  
  // Event trigger IE
  data.event_trigger = ba.buf;
  data.len_et = ba.len;

  // Action Definition IE
  data.action_def = NULL;
  data.len_ad = 0;

  return data;
}


static
 sm_ag_if_rd_t on_indication_tc_sm_ric(sm_ric_t const* sm_ric, sm_ind_data_t* data)
{
  assert(sm_ric != NULL); 
  assert(data != NULL); 
  sm_tc_ric_t* sm = (sm_tc_ric_t*)sm_ric;  

  sm_ag_if_rd_t rd_if = {.type =  TC_STATS_V0};

//  rd_if.tc_stats.hdr = tc_dec_ind_hdr(&sm->enc, data->len_hdr, data->ind_hdr);
  rd_if.tc_stats.msg = tc_dec_ind_msg(&sm->enc, data->len_msg, data->ind_msg);

  return rd_if;
}

static
sm_ctrl_req_data_t ric_on_control_req_tc_sm_ric(sm_ric_t const* sm_ric, const sm_ag_if_wr_t * data)
{
  assert(sm_ric != NULL); 
  assert(data != NULL); 
  assert(data->type == TC_CTRL_REQ_V0);

  sm_tc_ric_t* sm = (sm_tc_ric_t*)sm_ric;  


  byte_array_t ba = tc_enc_ctrl_hdr(&sm->enc,  &data->tc_req_ctrl.hdr );

  sm_ctrl_req_data_t ret_data = {0};  
  ret_data.ctrl_hdr = ba.buf;
  ret_data.len_hdr = ba.len;

  ba = tc_enc_ctrl_msg(&sm->enc, &data->tc_req_ctrl.msg);
  ret_data.ctrl_msg = ba.buf;
  ret_data.len_msg = ba.len;

  return ret_data;
}


static
 sm_ag_if_ans_t ric_on_control_out_tc_sm_ric(sm_ric_t const* sm_ric, const sm_ctrl_out_data_t * out)
{

  assert(sm_ric != NULL); 
  assert(out != NULL);

  sm_tc_ric_t* sm = (sm_tc_ric_t*)sm_ric;  

  sm_ag_if_ans_t ag_if = {.type = TC_AGENT_IF_CTRL_ANS_V0};  
  ag_if.tc = tc_dec_ctrl_out(&sm->enc, out->len_out, out->ctrl_out);

  //assert(ag_if.tc.len_diag > 0);

  return ag_if;
}


static
void ric_on_e2_setup_tc_sm_ric(sm_ric_t const* sm_ric, sm_e2_setup_t const* setup)
{
  assert(sm_ric != NULL); 
  assert(setup == NULL); 
//  sm_tc_ric_t* sm = (sm_tc_ric_t*)sm_ric;  

  assert(0!=0 && "Not implemented");
}

static
sm_ric_service_update_t on_ric_service_update_tc_sm_ric(sm_ric_t const* sm_ric, const char* data)
{
  assert(sm_ric != NULL); 
  assert(data != NULL); 
//  sm_tc_ric_t* sm = (sm_tc_ric_t*)sm_ric;  

  assert(0!=0 && "Not implemented");
}


void free_tc_sm_ric(sm_ric_t* sm_ric)
{
  assert(sm_ric != NULL);
  sm_tc_ric_t* sm = (sm_tc_ric_t*)sm_ric;
  free(sm);
}

//
// Allocation SM functions. The memory malloc by the SM is also freed by it.
//

static
void free_subs_data_tc_sm_ric(void* msg)
{
  assert(msg != NULL);
  assert(0!=0 && "Not implemented");
}

static
void free_ind_data_tc_sm_ric(void* msg)
{
  assert(msg != NULL);
  tc_ind_data_t* ind  = (tc_ind_data_t*)msg;
  free_tc_ind_hdr(&ind->hdr); 
  free_tc_ind_msg(&ind->msg); 
}

static
void free_ctrl_req_data_tc_sm_ric(void* msg)
{
  assert(msg != NULL);
  assert(0!=0 && "Not implemented");
}

static
void free_ctrl_out_data_tc_sm_ric(void* msg)
{
  assert(msg != NULL);
  assert(0!=0 && "Not implemented");
}

static
void free_e2_setup_tc_sm_ric(void* msg)
{
  assert(msg != NULL);
  assert(0!=0 && "Not implemented");
}

static
void free_ric_service_update_tc_sm_ric(void* msg)
{
  assert(msg != NULL);
  assert(0!=0 && "Not implemented");
}



sm_ric_t* make_tc_sm_ric(void /* sm_io_ric_t io */)
{
  sm_tc_ric_t* sm = calloc(1,sizeof(sm_tc_ric_t));
  assert(sm != NULL && "Memory exhausted");

  *((uint16_t*)&sm->base.ran_func_id) = SM_TC_ID; 


  sm->base.free_sm = free_tc_sm_ric;

  // Memory (De)Allocation
  sm->base.alloc.free_subs_data_msg = free_subs_data_tc_sm_ric; 
  sm->base.alloc.free_ind_data = free_ind_data_tc_sm_ric ; 
  sm->base.alloc.free_ctrl_req_data = free_ctrl_req_data_tc_sm_ric; 
  sm->base.alloc.free_ctrl_out_data = free_ctrl_out_data_tc_sm_ric; 
 
  sm->base.alloc.free_e2_setup = free_e2_setup_tc_sm_ric; 
  sm->base.alloc.free_ric_service_update = free_ric_service_update_tc_sm_ric; 

  // O-RAN E2SM 5 Procedures
  sm->base.proc.on_subscription = on_subscription_tc_sm_ric; 
  sm->base.proc.on_indication = on_indication_tc_sm_ric;

  sm->base.proc.on_control_req = ric_on_control_req_tc_sm_ric;
  sm->base.proc.on_control_out = ric_on_control_out_tc_sm_ric;

  sm->base.proc.on_e2_setup = ric_on_e2_setup_tc_sm_ric;
  sm->base.proc.on_ric_service_update = on_ric_service_update_tc_sm_ric; 
  sm->base.handle = NULL;

  assert(strlen(SM_TC_STR) < sizeof( sm->base.ran_func_name) );
  memcpy(sm->base.ran_func_name, SM_TC_STR, strlen(SM_TC_STR)); 

  return &sm->base;
}

