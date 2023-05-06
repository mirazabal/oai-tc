#ifndef TC_FIFO_QUEUE_H
#define TC_FIFO_QUEUE_H

#include "../queue.h"

queue_t* fifo_init(uint32_t id, void (*deleter)(void*));

/*
void fifo_free(queue_t*);

void fifo_push(queue_t*, void*, size_t);

void fifo_pop(queue_t*);

size_t fifo_size(queue_t*);

size_t fifo_bytes(queue_t*);

void* fifo_front(queue_t*);

//void* fifo_at(queue_t*, uint32_t);

void* fifo_end(queue_t*);

const char* fifo_name(queue_t*); 
*/

#endif

