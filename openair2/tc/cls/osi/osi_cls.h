/*
 * OSI based classifier for layers 3,4 and 7 
 */

#ifndef OSI_CLS_H
#define OSI_CLS_H

#include "../cls.h"

//__attribute__((malloc))
cls_t* osi_cls_init(void);  

/*
void osi_cls_free(osi_cls_t*);

void osi_cls_add(cls_t* cls_base, void* cls_data);

queue_t* osi_cls_dst_queue(cls_t* cls_base, const uint8_t* data, size_t size);

const char* osi_cls_name(cls_t* cls);
*/

#endif
