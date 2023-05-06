#ifndef TC_SCHED
#define TC_SCHED 

#include "../queue/queue.h"
#include "../tc_sm/ie/tc_data_ie.h"

typedef struct sch_s
{
  void (*free)(struct sch_s*); 

  void (*add_queue)(struct sch_s*, queue_t**);

  void (*del_queue)(struct sch_s*, queue_t**);

  queue_t* (*next_queue)(struct sch_s*);

  void (*pkt_fwd)(struct sch_s*);

  const char* (*name)(struct sch_s*);

  void* handle;

  // Statisitcs
  tc_sch_t (*stats)(struct sch_s*);

  // Modifications
  void (*mod)(struct  sch_s*, tc_mod_ctrl_sch_t const*);
  //void (*add)(struct  sch_s*, tc_add_ctrl_sch_t const*);
  //void (*del)(struct  sch_s*, tc_del_ctrl_sch_t const*);

} sch_t;

#endif

