#ifndef TC_QUEUE_H
#define TC_QUEUE_H

/*
 *Generic queue file. Nomenclature from the C++ std::queue class
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../tc_sm/ie/tc_data_ie.h"

typedef struct queue_s
{ 
  void (*free)(struct queue_s*);

  // Pointer to the packet and the size
  void (*push)(struct queue_s*, void*, size_t);

  void (*pop)(struct queue_s*);

  // Get number of packets
  size_t (*size)(struct queue_s*);

  // Get number of bytes
  size_t (*bytes)(struct queue_s*);

  /* Iterators */
  void* (*front)(struct queue_s*);

  void* (*end)(struct queue_s*);

  const char* (*name)(struct queue_s*);

  void* handle;

  // Queue ID
  uint32_t id;

  // Queue type e.g., CoDel, FIFO...
  tc_queue_e type;

  // Statistics
  tc_queue_t (*stats)(struct queue_s*); 

  // Modifications
  void (*mod)(struct queue_s*, tc_mod_ctrl_queue_t const*);
  //void (*add)(struct queue_s*, tc_add_ctrl_queue_t const*);
  //void (*del)(struct queue_s*, tc_del_ctrl_queue_t const*);

} queue_t;

#endif

