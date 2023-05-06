#ifndef TC_BDP_PCR
#define TC_BDP_PCR

#include "../pcr.h"
#include <pthread.h>
#include <stdbool.h>

pcr_t* bdp_pcr_init(void); 

/*
void bdp_pcr_free(pcr_t*);

pcr_act_e bdp_pcr_action(pcr_t*, uint32_t bytes); 

void bdp_pcr_bytes_fwd(pcr_t*, uint32_t bytes); 

void bdp_pcr_update(pcr_t*, uint32_t drb_size); 
*/

#endif

