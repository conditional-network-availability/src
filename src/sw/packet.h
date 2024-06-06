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

#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <endian.h>
#include <arch_helpers.h>

/* -----------------------------------------------------------------------------
* Ethernet Structures.
* Roughly based on: include/linux/if_ether.h
* --------------------------------------------------------------------------- */

#define ETH_ALEN	6		/* Octets in one ethernet addr */
#define ETH_HLEN	14		/* Total octets in header.	 */

#define ETH_P_IP 0x0800 /* Internet Protocol packet */
#define ETH_P_ARP 0x0806 /* Address Resolution packet        */

#define ETH_FCS_LEN 4 /* Frame Check Sequence at the end of a frame */

struct ethhdr {
	uint8_t h_dest[ETH_ALEN];	/* destination eth addr	*/
	uint8_t h_source[ETH_ALEN];	/* source ether addr	*/
	uint16_t h_proto;		/* packet type ID field, big-endian	*/
} __attribute__((packed));


/* -----------------------------------------------------------------------------
* Internet Protocol Structures
* Roughly based on netinet/ip.h.html and include/uapi/linux/in.h
* We are on a litt-endian system.
* --------------------------------------------------------------------------- */

#define IP_HLEN	20		/* Total octets in header. */

struct in_addr {
	uint32_t s_addr; // big endian
};

struct iphdr {
	uint8_t	ip_hl:4, ip_v:4;			/* header length and version */
	uint8_t	ip_tos;			/* type of service */
	uint16_t ip_len;			/* total length */
	uint16_t ip_id;			/* identification */
	int16_t ip_off;			/* fragment offset field */
	uint8_t ip_ttl;			/* time to live */
	uint8_t ip_p;			/* protocol */
	uint16_t ip_sum;			/* checksum */
	struct	in_addr ip_src,ip_dst;	/* source and dest address */
} __attribute__((packed));


/* -----------------------------------------------------------------------------
* TCP Structures
* Roughly based on include/uapi/linux/tcp.h
* We are on a litt-endian system.
* --------------------------------------------------------------------------- */

#define TCP_HLEN	20		// Total octets in header.

#define IP_P_ICMP 1
#define IP_P_TCP 6
#define IP_P_UDP 17

struct tcphdr {
	uint16_t	source; // big-endian
	uint16_t	dest; // big-endian
	uint32_t seq; // big-endian
	uint32_t ack_seq; // big-endian
	uint16_t res1:4, doff:4, fin:1, syn:1, rst:1, psh:1, ack:1, urg:1, ece:1, cwr:1;
	uint16_t window;
	uint16_t check;
	uint16_t urg_ptr;
};

/* -----------------------------------------------------------------------------
* UDP Structures
* Roughly based on include/uapi/linux/udp.h
* We are on a litt-endian system.
* --------------------------------------------------------------------------- */

#define UDP_HLEN	8		// Total octets in header.

struct udphdr {
	uint16_t source; // big-endian
	uint16_t	dest; // big-endian
	uint16_t len; // big-endian
	uint16_t check; // big-endian
};

/* -----------------------------------------------------------------------------
* Data Structure for Complete Header
* --------------------------------------------------------------------------- */

// #define TOTAL_HLEN ETH_HLEN + IP_HLEN + TCP_HLEN	/* Total octets in header. */
// #define TOTAL_HLEN ETH_HLEN + IP_HLEN	/* Total octets in header. */

struct ip_hdr_total {
	struct ethhdr eth;
	struct iphdr ip;
	// struct tcphdr tcp;
} __attribute__((packed));

struct udp_hdr_total {
	struct ethhdr eth;
	struct iphdr ip;
	struct udphdr udp;
} __attribute__((packed));

/* -----------------------------------------------------------------------------
* Helper Functions
* --------------------------------------------------------------------------- */

bool is_ipv4(struct ip_hdr_total* hdr);

#endif
