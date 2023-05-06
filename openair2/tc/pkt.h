#ifndef TRAFFIC_CONTROL_PACKET_H
#define TRAFFIC_CONTROL_PACKET_H

#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#define MTU_SIZE 1514

// Pkts Layout
typedef struct {
  uint8_t* data;
  size_t sz;

  uint64_t tstamp_cls;
  uint64_t tstamp_plc;
  uint64_t tstamp_q;
  uint64_t tstamp_schd;
  uint64_t tstamp_pcr;
  uint64_t tstamp_shp;

} pkt_t;

pkt_t* init_pkt(uint8_t* data, size_t size);

void free_pkt(void* p_v);

#endif

