#ifndef TC_SHP
#define TC_SHP

#include "../tc_sm/ie/tc_data_ie.h"

#include "../alg_ds/ds/seq_container/seq_generic.h"
#include "../alg_ds/meter/meter.h"

#include "../queue/queue.h" 

#include <stdbool.h>
#include <stdint.h>

typedef struct shp_s
{
  mtr_t m;

  uint32_t max_rate_kbps;
  bool active; // a shaper per queue always exists
} shp_t;

shp_t* shp_init(uint32_t time_window_us, uint32_t max_rate_kbps);

void shp_free(shp_t* s);

// Inform the shaper that pkts have been dequeued
void shp_bytes_fwd(shp_t* s, uint32_t  bytes);

typedef enum shp_act{
    SHP_WAIT,
    SHP_PASS
} shp_act_e;

shp_act_e shp_action(shp_t* s, uint32_t pkt_size); // possible

tc_shp_t shp_stat(shp_t* s); 

void shp_mod(shp_t* s, tc_mod_ctrl_shp_t const* ctrl);


/*
// Optional Shaper type 
typedef struct opt_shp_s
{
  shp_t s;
  bool has_value;
} opt_shp_t;
*/



#endif

