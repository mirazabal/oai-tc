#ifndef STO_CLS_H
#define STO_CLS_H

/*
 * Stocastic Classifier. It assigns the packets to the queues according to a jenkins hashing
 */

#include "../cls.h"


cls_t* sto_cls_init(void);

#endif

