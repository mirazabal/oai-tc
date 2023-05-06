#ifndef TC_ECN_CODEL_QUEUE_H
#define TC_ECN_CODEL_QUEUE_H

#include "../queue.h"
#include <stdlib.h>

//
// Implementation of a CoDel queue following 
// the RFC 8289 document with ecn instead of dropping the packet
// https://datatracker.ietf.org/doc/html/rfc8289
//

queue_t* ecn_codel_init(uint32_t id, void (*deleter)(void*));


#endif

