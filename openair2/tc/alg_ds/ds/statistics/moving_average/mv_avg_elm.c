/*
MIT License

Copyright (c) 2021 Mikel Irazabal

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#include "mv_avg_elm.h"
#include <assert.h>
#include "../../../alg/accumulate.h"


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

