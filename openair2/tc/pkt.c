#include "pkt.h"

#include <stdlib.h>
#include <string.h>

pkt_t* init_pkt(uint8_t* data, size_t size)
{
  assert(data != NULL);
  assert(size < MTU_SIZE + 1);
  pkt_t* p = calloc(1, sizeof(pkt_t));
  assert(p != NULL);
  p->data = calloc(1, size);
  assert(p->data != NULL && "Memory exhasuted");
  memcpy(p->data, data, size);
//  p->data = data;
  p->sz = size;
  return p;
}

void free_pkt(void* p_v)
{
  assert(p_v != NULL);
  pkt_t* p = (pkt_t*)p_v;
  free(p->data);
  free(p);
}

