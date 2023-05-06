#ifndef CLS_H
#define CLS_H

#include "../queue/queue.h"
#include "../tc_sm/ie/tc_data_ie.h"

#include <stddef.h>
#include <stdint.h>

typedef struct cls_s
{ 
  void (*free)(struct cls_s* cls);

  // Way of communicating with the classifier. Every classifier could have its special semantics  
  // void (*conf)(struct cls_s* cls, void* cls_data);

  void (*add_queue)(struct cls_s* cls, queue_t**);
 
  void (*del_queue)(struct cls_s* cls, queue_t**);
  
  // get destination queue for pkt in the cls
  queue_t* (*dst_q)(struct cls_s* cls, const uint8_t* data, size_t size);

  // inform the classifier that the packet was enqueued, e.g., not dropped 
  void (*pkt_fwd)(struct cls_s*);

 // name of the classifier
 const char* (*name)(struct cls_s*);

 void* handle;

 tc_cls_t (*stats)(struct cls_s*);

 void (*add)(struct cls_s*, tc_add_ctrl_cls_t const* );
 void (*mod)(struct cls_s*, tc_mod_ctrl_cls_t const* );
 void (*del)(struct cls_s*, tc_del_ctrl_cls_t const* );

} cls_t;

#endif

