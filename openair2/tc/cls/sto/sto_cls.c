#include "sto_cls.h"

#include "../../alg_ds/ds/seq_container/seq_generic.h"
#include "../../alg_ds/alg/alg.h"
#include "../../queue/queue.h"
#include <stdint.h>

#include <assert.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

typedef struct sto_cls_s{
  cls_t base;
  seq_arr_t queues; // queue_t*
  queue_t* last;
} sto_cls_t;


static inline 
uint32_t murmur_32_scramble(uint32_t k) 
{
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

static
uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed)
{
	uint32_t h = seed;
    uint32_t k;
    /* Read in groups of 4. */
    for (size_t i = len >> 2; i; i--) {
        // Here is a source of differing results across endiannesses.
        // A swap here has no effects on hash properties though.
        memcpy(&k, key, sizeof(uint32_t));
        key += sizeof(uint32_t);
        h ^= murmur_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }
    /* Read the rest. */
    k = 0;
    for (size_t i = len & 3; i; i--) {
        k <<= 8;
        k |= key[i - 1];
    }
    // A swap is *not* necessary here because the preceding loop already
    // places the low bytes in the low places according to whatever endianness
    // we use. Swaps only apply when the memory is copied in a chunk.
    h ^= murmur_32_scramble(k);
    /* Finalize. */
	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

static
void sto_cls_free(cls_t* cls_base)
{
  sto_cls_t* cls = (sto_cls_t*)cls_base; 
  void* non_owning = NULL;
  seq_free(&cls->queues, non_owning); 
  free(cls);
}

static
void sto_cls_add(cls_t* cls_base, tc_add_ctrl_cls_t const* add )
{
  assert( cls_base != NULL );
  assert( add != NULL );
  assert(0!=0 && "Not implemented!!");
}

static
void sto_cls_mod(cls_t* cls_base, tc_mod_ctrl_cls_t const* mod )
{
  assert( cls_base != NULL );
  assert( mod != NULL );

  assert(0!=0 && "Not implemented!!");
}

static
void sto_cls_del(cls_t* cls_base, tc_del_ctrl_cls_t const* del )
{
  assert( cls_base != NULL );
  assert( del != NULL );

  assert(0!=0 && "Not implemented!!");
}



static
void sto_cls_add_queue(cls_t* cls_base, queue_t** q)
{
  assert(cls_base != NULL);
  assert(q != NULL);
  sto_cls_t* cls = (sto_cls_t*)cls_base;
  seq_push_back(&cls->queues, q, sizeof(queue_t*));
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
void sto_cls_del_queue(cls_t* cls_base, queue_t** q)
{
  assert(cls_base != NULL);
  assert(q != NULL);

  sto_cls_t* cls = (sto_cls_t*)cls_base;

  void* it = find_if_r(&cls->queues, q, eq_queue_pointer);
  void* end = seq_end(&cls->queues);
  assert(it != end && "Queue not found");

  void* next = seq_next(&cls->queues, it);
  seq_erase(&cls->queues, it, next );
}

typedef struct{
  uint32_t saddr;
  uint32_t daddr;
  uint16_t sport;
  uint16_t dport;
  uint8_t protocol;
} five_tuple_t;

static
five_tuple_t extract_five_tuple(uint8_t const* data, size_t sz)
{
  assert(data != NULL);
  assert(sz > sizeof(struct iphdr));
  assert(sz > sizeof(struct tcphdr));
  assert(sz > sizeof(struct udphdr));

  five_tuple_t ans = {0}; 

  struct iphdr* hdr = (struct iphdr*) data;
   ans.saddr = hdr->saddr;
   ans.daddr = hdr->daddr;
   ans.protocol = hdr->protocol;

  switch (hdr->protocol) {
  case IPPROTO_TCP: {
//    struct tcphdr *tcp = (struct tcphdr *)(data + sizeof(struct iphdr));

    struct tcphdr* tcp = (struct tcphdr*)((uint32_t*)hdr + hdr->ihl);

//    struct in_addr paddr;
//    paddr.s_addr = hdr->saddr;
//    char *strAdd2 = inet_ntoa(paddr);
//    printf("IP source address %s \n", strAdd2  );
//    paddr.s_addr = hdr->daddr;
//    strAdd2 = inet_ntoa(paddr);
//    printf("IP dst address %s \n", strAdd2  );

    ans.sport = ntohs(tcp->source);
    ans.dport = ntohs(tcp->dest);
//    printf("Ingress TCP seq_number %u src %d dst %d \n", ntohl(tcp->seq), ans.sport, ans.dport);
    break;
  }
  case IPPROTO_UDP: {
    struct udphdr *udp = (struct udphdr*)((uint32_t*)hdr + hdr->ihl);
    ans.sport = ntohs(udp->source);
    ans.dport = ntohs(udp->dest);
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

  return ans;
}

static
queue_t* sto_cls_dst_queue(cls_t* cls_base, const uint8_t* data, size_t size)
{
  sto_cls_t* cls = (sto_cls_t*)cls_base;

  five_tuple_t tup = extract_five_tuple(data, size);

  uint32_t seed = 724553; // prime number 
  uint32_t hash = murmur3_32((uint8_t*)&tup, sizeof(tup), seed); // general purpose hashing function. See https://en.wikipedia.org/wiki/MurmurHash
  const size_t num_queues = seq_size(&cls->queues);
  size_t pos = hash % num_queues; 

  // Shitty, just for visualization purposes
  if(tup.sport > 9299 && tup.sport < 9350 && num_queues > 1){
    pos = 1; 
    printf( "Sto_cls protocol %d src_addr %d dst_add %d src_port %d dst_port %d to queue %ld \n",tup.protocol, tup.saddr, tup.daddr, tup.sport, tup.dport, pos);
//    printf("ICMP traffic detected going to queue id = %lu \n", pos); 
  } else {
     pos = 0; 
    printf( "Sto_cls  protocol %d src_addr %d dst_add %d src_port %d dst_port %d to queue %ld \n",tup.protocol, tup.saddr, tup.daddr, tup.sport, tup.dport, pos);
  }

  return *(queue_t**)seq_at(&cls->queues, pos);
}

static
void sto_cls_pkt_fwd(cls_t* cls_base)
{
  assert(cls_base != NULL);
  sto_cls_t* cls = (sto_cls_t*)cls_base;
  (void)cls; // noop
  //assert(0!=0 && "Not implemented!!");
}

static
tc_cls_t sto_cls_stats(cls_t* cls_base)
{
  assert(cls_base != NULL);
  sto_cls_t* cls = (sto_cls_t*)cls_base;
  (void)cls;

  tc_cls_t ans = {.type = TC_CLS_STO };
  ans.sto.dummy = 42;
  return ans;
}

static
const char* sto_cls_name(cls_t* cls)
{
  assert(cls != NULL);
  return "Stocastic Classifier";
}


cls_t* sto_cls_init(void)
{
  sto_cls_t* cls = malloc(sizeof(sto_cls_t));
  assert(cls != NULL);

  cls->base.dst_q = sto_cls_dst_queue;
  cls->base.name = sto_cls_name;
  cls->base.handle = NULL;
  cls->base.free = sto_cls_free;
  cls->base.add_queue = sto_cls_add_queue;
  cls->base.del_queue = sto_cls_del_queue;

  cls->base.pkt_fwd = sto_cls_pkt_fwd;
  cls->base.stats = sto_cls_stats;

  cls->base.add = sto_cls_add;
  cls->base.mod = sto_cls_mod;
  cls->base.del = sto_cls_del;


  cls->last = NULL;

  seq_init(&cls->queues, sizeof(queue_t*));
  return (cls_t*)&cls->base; 
} 


