#ifndef TC_PCR
#define TC_PCR

#include <stddef.h>
#include <stdint.h>
#include "../tc_sm/ie/tc_data_ie.h"

typedef enum pcr_act{
  PCR_WAIT,
  PCR_PASS,
} pcr_act_e;

typedef struct pcr_s
{
  void (*free)(struct pcr_s*);

  pcr_act_e (*action)(struct pcr_s*, uint32_t data_size); 

  void (*bytes_fwd)(struct pcr_s*, uint32_t); 

//  void (*update)(struct pcr_s*, uint32_t drb_size); 
  tc_pcr_e type;

  void* handle;

  // Statistics
  tc_pcr_t (*stats)(struct pcr_s*);

  // Modifications
  void (*mod)(struct pcr_s*, tc_mod_ctrl_pcr_t const*);
  //void (*add)(struct pcr_s*, tc_add_ctrl_pcr_t const*);
  //void (*del)(struct pcr_s*, tc_del_ctrl_pcr_t const*);

} pcr_t;

#endif

