#ifndef TC_CODEL_QUEUE_H
#define TC_CODEL_QUEUE_H

#include "../queue.h"
#include <stdlib.h>

//
// Implementation of a CoDel queue following 
// the RFC 8289 document 
// https://datatracker.ietf.org/doc/html/rfc8289
//

queue_t* codel_init(uint32_t id, void (*deleter)(void*));

/*
void codel_free(queue_t*);

void codel_push(queue_t*, void*, size_t);

void codel_pop(queue_t*);

size_t codel_size(queue_t*);

size_t codel_bytes(queue_t*);

void* codel_front(queue_t*);

// void* codel_at(queue_t*, uint32_t);

void* codel_end(queue_t*);

const char* codel_name(queue_t*); 
*/

#endif

