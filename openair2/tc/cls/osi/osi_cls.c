#include "../../alg_ds/alg/alg.h"
#include "../../alg_ds/alg/find.h"
#include "../../alg_ds/ds/seq_container/seq_generic.h"

#include "osi_cls.h"

#include <assert.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

static const int32_t flt_wildcard = -1;

/*
// OSI L3 filter
typedef struct L3_filter_s{
  const int32_t src_addr; // filter wildcard (i.e., -1) indicates matches all
  const int32_t dst_addr;
} L3_filter_t;

// OSI L4 filter
typedef struct L4_filter_s{
  const int32_t src_port;
  const int32_t dst_port;
  const int32_t protocol;
} L4_filter_t;

// OSI L7 filter
typedef struct L7_filter_s{
  //TODO: Add the OSI Layer 7 
} L7_filter_t;

// Complete filter
typedef struct filter_s{
  const L3_filter_t l3;
  const L4_filter_t l4;
  const L7_filter_t l7;
  const int32_t dst_queue;
} filter_t;

//L3_filter_t L3_filter_init(int32_t src_address , int32_t dst_address);
//L4_filter_t L4_filter_init(int32_t src_port, int32_t dst_port, int32_t proto);
//L7_filter_t L7_filter_init(void);

bool pkt_matches_filter(const struct iphdr *hdr, const filter_t *flt);

__attribute__((malloc))
filter_t* filter_init(L3_filter_t l3, L4_filter_t l4, L7_filter_t l7, int32_t dst_queue);

void filter_free(filter_t*);
*/


typedef struct osi_cls_s{
  cls_t base;
  seq_arr_t filters; // of filter_t*
  seq_arr_t queues; // queue_t*
  uint32_t flt_id;
} osi_cls_t;


static
bool pkt_matches_L3_filter(const struct iphdr *hdr, const L3_filter_t *flt) {
  assert(hdr != NULL);
  assert(flt != NULL);
  assert(flt->src_addr > -2 && "Only -1 allowed as wildcard. Other negative values are not valid " );

  const bool match_src_addr =
      flt->src_addr == flt_wildcard ? true : hdr->saddr == flt->src_addr;
  const bool match_dst_addr =
      flt->dst_addr == flt_wildcard ? true : hdr->daddr == flt->dst_addr;
  return match_src_addr && match_dst_addr;
}

static
bool pkt_matches_L4_filter(const struct iphdr *hdr, const L4_filter_t *flt) {
  uint16_t src_port = 0;
  uint16_t dst_port = 0;
  switch (hdr->protocol) {
  case IPPROTO_TCP: {

    struct tcphdr* tcp = (struct tcphdr*)((uint32_t*)hdr + hdr->ihl);

    src_port = ntohs(tcp->source);
    dst_port = ntohs(tcp->dest);
    break;
  }
  case IPPROTO_UDP: {
    struct udphdr *udp = (struct udphdr *)((uint32_t*)hdr + hdr->ihl);
    src_port = ntohs(udp->source);
    dst_port = ntohs(udp->dest);
    break;
  }
  case IPPROTO_ICMP: {
    //src_port = -1;
    //dst_port = -1;
    break;
  }
  default:
    assert(0 != 0 && "No foreseen protocol detected");
  }
  const uint32_t match_src_port =
      flt->src_port == flt_wildcard ? true : src_port == flt->src_port;
  printf(" match_src_port %d flt_wildcard %d  src_port %d flt->src_port %d \n",  match_src_port, flt_wildcard, src_port , flt->src_port);
  const uint32_t match_dst_port =
      flt->dst_port == flt_wildcard ? true : dst_port == flt->dst_port;
  const uint32_t match_proto =
      flt->protocol == flt_wildcard ? true : hdr->protocol == flt->protocol;

  return match_proto && match_src_port && match_dst_port;
}

static
bool pkt_matches_L7_filter(const uint8_t *pkt, size_t size, const L7_filter_t *flt) {
  (void)pkt;
  (void)size;
  (void)flt;
  return true;
}

bool pkt_matches_filter(const struct iphdr *hdr, tc_cls_osi_filter_t const* flt) 
{

  if(hdr->protocol == IPPROTO_TCP) {

   struct tcphdr* tcp = (struct tcphdr*)((uint32_t*)hdr + hdr->ihl);

    struct in_addr paddr;
    paddr.s_addr = hdr->saddr;

    char *strAdd2 = inet_ntoa(paddr);
    printf("IP source address %s \n", strAdd2  );

    paddr.s_addr = hdr->daddr;
    strAdd2 = inet_ntoa(paddr);

    printf("IP dst address %s \n", strAdd2  );

    uint16_t const sport = ntohs(tcp->source);
    uint16_t const dport = ntohs(tcp->dest);
    printf("OSI CLS src %d dst %d \n", sport, dport);
  } 


  bool l3 = pkt_matches_L3_filter(hdr, &flt->l3);
  bool l4 = pkt_matches_L4_filter(hdr, &flt->l4);
  bool l7 = pkt_matches_L7_filter((const uint8_t *)hdr, 0, &flt->l7);
  return l3 && l4 && l7;
}

//__attribute__((malloc))
/*
filter_t* filter_init(L3_filter_t l3, L4_filter_t l4, L7_filter_t l7, int32_t dst_queue){
 assert(dst_queue > -1 && "Queues cannot have negative index");
 filter_t tmp = {.l3 = l3, .l4=l4, .l7 = l7, .dst_queue = dst_queue};
 filter_t* flt = malloc(sizeof(filter_t)); 
 assert(flt != NULL && "Memory exhausted!");
 memcpy(flt,&tmp,sizeof(tmp));  
 return flt;
}

void filter_free(filter_t* flt)
{
  assert(flt != NULL);
  free(flt);
};

*/

static
void osi_cls_free(cls_t* cls_base )
{
  assert( cls_base != NULL );
  osi_cls_t* cls = (osi_cls_t*)(cls_base);
  void* non_owning = NULL;
  seq_free(&cls->filters, non_owning);
  seq_free(&cls->queues, non_owning);
  free(cls);
}
/*
static
void osi_cls_conf(cls_t* cls_base, void* cls_data)
{
  assert(cls_base != NULL);
  assert(cls_data != NULL);
  filter_t* flt = (filter_t*)(cls_data);
  osi_cls_t* cls = (osi_cls_t*)(cls_base);
  seq_push_back(&cls->filters, (uint8_t*)flt, sizeof(filter_t));
}
*/

static
void osi_cls_add(cls_t* cls_base, tc_add_ctrl_cls_t const* add )
{
  assert(cls_base != NULL);
  osi_cls_t* cls = (osi_cls_t*)(cls_base);
 
 tc_cls_osi_filter_t f = {.id = cls->flt_id++,
                          .l3 = add->osi.l3 ,
                           .l4 = add->osi.l4,
                          //.l7 = add->osi.l7,
                            .dst_queue = add->osi.dst_queue}; 

  seq_push_back(&cls->filters, (uint8_t*)&f, sizeof(tc_cls_osi_filter_t));
}

static
bool eq_filter_id(void const* value_v, void const* it)
{
  assert(value_v != NULL);
  assert(it != NULL);

  tc_cls_osi_filter_t* f = (tc_cls_osi_filter_t*)it;
  uint32_t* id = (uint32_t*)value_v;

  if(*id == f->id)
    return true;

  return false;
}

static
void osi_cls_mod(cls_t* cls_base, tc_mod_ctrl_cls_t const* mod )
{
  assert(cls_base != NULL);
  assert(mod != NULL);
  osi_cls_t* cls = (osi_cls_t*)(cls_base);

  void* it = find_if_r(&cls->filters, (uint32_t*)&mod->osi.filter.id , eq_filter_id );
  assert(it != seq_end(&cls->filters) && "Filter id not found" );

  tc_cls_osi_filter_t* f = (tc_cls_osi_filter_t*)it;

  f->dst_queue = mod->osi.filter.dst_queue;
  f->l3 = mod->osi.filter.l3;
  f->l4 = mod->osi.filter.l4;

  printf("Modification took place succesfully \n ");

}

static 
void osi_cls_del(cls_t* cls_base, tc_del_ctrl_cls_t const* del )
{
  assert(cls_base != NULL);
  assert(del != NULL);
  osi_cls_t* cls = (osi_cls_t*)(cls_base);

  assert(0!=0 && "not implemented");
  (void)cls;
}

inline static
bool match_filter(const void* value, const void* filter)
{
 struct iphdr* ip =  (struct iphdr*)value;
 tc_cls_osi_filter_t* f = (tc_cls_osi_filter_t*)filter;  





 return pkt_matches_filter(ip, f);
}

static
bool eq_queue_id(void const* value_v, void const* it)
{
  assert(value_v != NULL);
  assert(it != NULL);

  uint32_t* id = (uint32_t*) value_v;
  queue_t* q = *(queue_t**)it;

  return *id == q->id;
}

static
queue_t* osi_cls_dst_queue(cls_t* cls_base, const uint8_t* data, size_t size)
{
  assert(cls_base != NULL);
  assert(data != NULL);
  osi_cls_t* cls = (osi_cls_t*)(cls_base);
  assert(seq_size(&cls->queues) > 0);
  assert(size > sizeof(struct iphdr) );
  assert(size > sizeof(struct tcphdr) );
  assert(size > sizeof(struct udphdr) );

  if(seq_size(&cls->filters) == 0) {
    //printf("[OSI]: No filters found, early return \n");
    return *(queue_t**)seq_front(&cls->queues);
  }

  void* it = seq_front(&cls->filters);
  void* end = seq_end(&cls->filters);

  while(it != end){
    tc_cls_osi_filter_t* f = (tc_cls_osi_filter_t*)it;    

    if(pkt_matches_filter((struct iphdr const *)data, f) == true){
      // find queue id
      void* it = find_if_r(&cls->queues, &f->dst_queue, eq_queue_id); 
      assert(it != seq_end(&cls->queues));

      printf("Pkt match, queue id = %d \n",  (*(queue_t**)it)->id );
      return *(queue_t**)it;
    }
    it = seq_next(&cls->filters, it);
  }

  printf("[OSI]: No filters found\n");
  return *(queue_t**)seq_front(&cls->queues);
}

static
void osi_cls_add_queue(struct cls_s* cls_base, queue_t** q)
{
  assert(cls_base != NULL);
  assert(q != NULL);
  osi_cls_t* cls = (osi_cls_t*)(cls_base);

  seq_push_back(&cls->queues, q, sizeof(queue_t*));

  printf("Adding queue %p to the OSI clasifier \n", *q);
}

static
bool eq_queue_pointer(void const* value_v, void const* it)
{
  assert(value_v != NULL);
  assert(it != NULL);

  queue_t* q0 = *(queue_t**)value_v;
  queue_t* q1 = *(queue_t**)it;

  return q0 == q1;
}

static
void osi_cls_del_queue(struct cls_s* cls_base, queue_t** q)
{
   assert(cls_base != NULL);
   assert(q != NULL);
   osi_cls_t* cls = (osi_cls_t*)(cls_base);

  void* it = find_if_r(&cls->queues, q, eq_queue_pointer);
  void* end = seq_end(&cls->queues);
  assert(it != end && "Queue not found");

  void* next = seq_next(&cls->queues, it);
  seq_erase(&cls->queues, it, next );
}


static
void osi_pkt_fwd(cls_t* cls)
{
  assert(cls != NULL);
  (void)cls;
  //assert(0!=0 && "not implemented");
}

static
tc_cls_t osi_cls_stats(cls_t* cls_base)
{
  assert(cls_base != NULL);
  osi_cls_t* cls = (osi_cls_t*)cls_base;

  tc_cls_t ans = {.type = TC_CLS_OSI };
  ans.osi.len = seq_size(&cls->filters); 

  if(ans.osi.len > 0 ){
    ans.osi.flt = calloc(ans.osi.len, sizeof(tc_cls_osi_filter_t));
    assert(ans.osi.flt != NULL && "Exhausted memory");
  }

  uint32_t idx = 0;
  void* it = seq_front(&cls->filters);
  void const* end = seq_end(&cls->filters);

  while(it != end){
    tc_cls_osi_filter_t* f = (tc_cls_osi_filter_t*)it;
    ans.osi.flt[idx] = cp_tc_cls_osi_filter(f);
    it = seq_next(&cls->filters, it);
    ++idx;
  }

  return ans;
}

static
const char* osi_cls_name(cls_t* cls)
{
  (void)cls;
  return "OSI classifier";
}

//__attribute__((malloc))
cls_t* osi_cls_init(void)
{
  osi_cls_t* cls = malloc(sizeof(osi_cls_t));
  assert(cls != NULL);

//  cls->base.conf = osi_cls_conf;
  cls->base.dst_q = osi_cls_dst_queue;
  cls->base.name = osi_cls_name;
  cls->base.handle = NULL;
  cls->base.free = osi_cls_free;

  cls->base.add_queue = osi_cls_add_queue;
  cls->base.del_queue = osi_cls_del_queue;

  cls->base.pkt_fwd = osi_pkt_fwd;
  cls->base.stats = osi_cls_stats;

  cls->base.add = osi_cls_add; 
  cls->base.mod = osi_cls_mod; 
  cls->base.del = osi_cls_del; 

  seq_init(&cls->filters,sizeof(tc_cls_osi_filter_t));
  seq_init(&cls->queues, sizeof(queue_t*));

  cls->flt_id = 0;

  return (cls_t*)cls;
}  

