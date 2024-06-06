/*******************************************************************************
* Copyright (C) 2022-2024 Jonas Röckl <joroec@gmx.net>
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; If not, see <http://www.gnu.org/licenses/>.
*******************************************************************************/

#include "selector.h"
#include "packet.h"
#include "app/app.h"
#include "asm/memcpy.h"

#include <string.h>
#include <common/debug.h>
#include <endian.h>
#include <arch_helpers.h>

static void put_into_pta_receive_ring(uint8_t* packet, size_t total_size)  {

  bool free = pta_rx_ring_free[pta_rx_ring_insert_pos];
  if(!free) // {
    // // ERROR("Ring is not free! Too much RX! Drop packet\n");
    return;
  // }

  pta_rx_ring_free[pta_rx_ring_insert_pos] = false;
  pta_rx_ring_size[pta_rx_ring_insert_pos] = total_size;

  /* INFO("Received packet (total_size: 0x%lx): \n", total_size);
  uint32_t j;
  for(j=0; j<total_size; j++) {
    printf("%02x ", packet[j]);
  }
  printf("\n"); */

  uint8_t* dst = pta_rx_buffer_ring->elements[pta_rx_ring_insert_pos].data;
  memcpy_aarch64(dst, packet, total_size);

  pta_rx_ring_insert_pos = (pta_rx_ring_insert_pos + 1) % DATARING_ELEMENTS_N;
}


/* -----------------------------------------------------------------------------
* Public Interface Declaration
* --------------------------------------------------------------------------- */

bool ___selector_run(uint8_t* packet, size_t total_size) {
  struct udp_hdr_total* hdr = (struct udp_hdr_total*) packet;

  // Every UDP packet FROM port 6666 belongs to SW
  uint16_t dest_port = be16toh(hdr->udp.dest);
  if(dest_port == 6666) {
    // Consume packet
    put_into_pta_receive_ring(packet, total_size);

    // Tell the framework that we have consumed the packet
    return true;
  }

  return false;
}

bool __selector_run(uint8_t* packet, size_t total_size) {
  // We know that the header is long enough so that it could be an IP header.
  // Thus, the cast is safe.
  struct ip_hdr_total* hdr = (struct ip_hdr_total*) packet;

  if(!is_ipv4(hdr)) // {
    return false; // not for the SW
  // }

  // Are we an UDP packet?
  if(total_size < ETH_HLEN + IP_HLEN + UDP_HLEN) // {
    // ERROR("Not even an UDP packet: %ld\n", total_size);
    return false; // not for SW
  // }

  return ___selector_run(packet, total_size);
}

bool _selector_run(uint8_t* packet, size_t total_size) {

  // packet is as least as big as a complete Ethernet header, thus this cast is safe
  // struct ethhdr* eth_hdr = (struct ethhdr*) packet;

  // Besides from ARP traffic, we only consider IP traffic. We want to destroy
  // anything else.
  if(total_size < ETH_HLEN + IP_HLEN) // {
    // ERROR("Not even an IP packet: %ld\n", total_size);
    return false; // not for SW
  // }

  return __selector_run(packet, total_size);
}

// returns true if packet was for SW
bool selector_run(uint8_t* packet, size_t total_size) {

  // We need this since the headers are unanligned in the moment the padding for
  // the inbound/outbound packets is dropped.
  write_sctlr_el3(read_sctlr_el3() & ~SCTLR_A_BIT);

  // If the PTA ring is full, we return true here. In the consequence, the
  // frame is DESTROYED in the calling function. This is so that
  // a flooding attack cannot selectively disable NW receiving traffic.
  bool free = pta_rx_ring_free[pta_rx_ring_insert_pos];
  if(!free)
    return true;

  // This is an ugly hack, since our NIC aligns the packet headers:
  // When ENETn_RACC[SHIFT16] is set, each frame is received with two additional bytes
  // in front of the frame.
  // The user application must ignore these first two bytes and find the first byte of the frame
  // in bits 23–16 of the first word from the RX FIFO.
  if(total_size < ETH_HLEN + 2) // {
    // ERROR("Not even an Ethernet packet: %ld\n", total_size);
    return false; // not for the SW
  // }

  // The first two bytes are added by the hardware
  packet = packet + 2;
  total_size = total_size - 2;

  return _selector_run(packet, total_size);
}
