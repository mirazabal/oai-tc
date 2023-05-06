#include "mv_avg_elm.h"
#include <assert.h>
#include "../../../alg/accumulate.h"
#

void mv_avg_elm_init(mv_avg_elm_t* m, uint32_t elm)
{
  assert(m != NULL);
  assert(elm > 0);

  seq_init(&m->val, sizeof(uint32_t));

  m->elm = elm;
  m->avg = 0.0;
}

void mv_avg_elm_free(mv_avg_elm_t* m)
{
  assert(m != NULL);
  void* value_semantic = NULL;
  seq_free(&m->val, value_semantic);
}

void mv_avg_elm_push_back(mv_avg_elm_t* m, uint32_t val)
{
  assert(m != NULL);

  size_t const elm = seq_size(&m->val);
  seq_push_back(&m->val, (uint8_t*)&val, sizeof(val));

  m->avg = (m->avg*elm + val)/(elm+1);
  //+ val/(elm+1);  
}

double mv_avg_elm_val(mv_avg_elm_t* m)
{
  assert(m != NULL);

  size_t const sz = seq_size(&m->val);

  if(sz > m->elm){
    void* f_val = seq_front(&m->val);
    void* l_val = seq_at(&m->val, sz - m->elm); 
    uint64_t const acc = accumulate_ring(&m->val,f_val, l_val);
    seq_erase(&m->val, f_val, l_val);
    assert(seq_size(&m->val) == m->elm && "Num. elm adjusted");
    m->avg = (m->avg*sz - acc)/(m->elm);
  }
  
  return m->avg;
}

