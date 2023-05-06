#ifndef oai_3gpp_38_331
#define oai_3gpp_38_331

#include "ds/queues/queue_generic.h"

// 3gpp 38.331 maxQFI 
// maxQFI INTEGER ::= 63
#define maxQFI 64
// 3gpp 38.331: maxDRB
// INTEGER ::= 29 -- Maximum number of DRBs (that can be added in DRB-ToAddModLIst)
#define maxDRB 30



// I don't like this...
typedef queue_arr_t qdisc_queue_t; 


#endif


