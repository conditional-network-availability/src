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

#include "app.h"

#include "../state.h"
#include "../packet.h"
#include "../descring.h"

#include <lib/mmio.h>

#define BENCHMARK_THROUGHPUT
#define FLOODER
// #define BENCHMARK_LATENCY_2
#define RX_BENCH_DURATION_S 10

/* -----------------------------------------------------------------------------
* Global State
* --------------------------------------------------------------------------- */

struct dataring* pta_rx_buffer_ring = (struct dataring*) PTA_RX_BUFFER_BASE;
uint32_t pta_rx_ring_insert_pos = 0; // pos to insert next data
uint32_t pta_rx_ring_read_pos = 0; // pos to read next data
bool pta_rx_ring_free[DATARING_ELEMENTS_N]; // element is free
uint32_t pta_rx_ring_size[DATARING_ELEMENTS_N]; // element is free

struct dataring* pta_tx_buffer_ring = (struct dataring*) PTA_TX_BUFFER_BASE;
uint32_t pta_tx_ring_pos = 0; // pos to poll next data
bool tx_bench_running = false; // set by kernel module txctl via SMC

uint16_t tx_ip_id = 0;

#define TOTAL_TX_SIZE 1490

// uint64_t remaining_rx_bench_duration = 0; // in ms
uint64_t rx_bench_bytes = 0;
bool rx_bench_running = false;
uint64_t rx_bench_started = 0;

/* -----------------------------------------------------------------------------
* Flags for the buffer descriptor fields
* --------------------------------------------------------------------------- */

// desc.cbd_sc (Control and status info)
#define BD_SC_WRAP				((uint16_t)0x2000)	/* Last buffer descriptor */
#define BD_ENET_TX_READY	    ((uint16_t)0x8000) /* Ready to transmit */
#define BD_ENET_TX_LAST		    ((uint16_t)0x0800) /* Last BD in frame */
#define BD_ENET_TC		        ((uint16_t)0x0400) /* Transmit CRC after last data byte */
#define BD_ENET_RX_EMPTY	    ((uint16_t)0x8000) /* RX entry empty */
#define BD_ENET_RX_OV		    ((uint16_t)0x0002) /* A receive FIFO overrun occurred */

//cbd_esc field
#define BD_ENET_RX_INT		    ((uint32_t)0x00800000) /* EXTENDED BD: IRQ when RX */
#define BD_ENET_PROT_CHECKSUM   ((uint32_t)0x10000000)
#define BD_ENET_IP_CHECKSUM     ((uint32_t)0x08000000)

// ENET_TDAR2: Written by the user to indicate TX ring 2 has been updated with new buffers.
#define ENET_TDAR2 U(0x1EC)

//cbd_prot field

#define BD_RX_MAKE_HDR_LEN(v)	((v << 27) & 0x1f)
#define BD_RX_MAKE_PROT_TYPE(v)	((v << 16) & 0xff)
#define BD_RX_MAKE_CHKSUM(v)	(v & 0xffff)


/* -----------------------------------------------------------------------------
* Function Definitions
* --------------------------------------------------------------------------- */

void pta_init(void) {

	// TODO: Initialize BD ring in SW. So far, this is done in NW but then
	// the ring is not used any longer in NW.

	for(uint32_t i=0; i<DATARING_ELEMENTS_N; i++) {
		pta_rx_ring_free[i] = true;
	}

	INFO("Initialize send packets\n");
	for(uint32_t i=0;i<DATARING_ELEMENTS_N;i++) {
		uint8_t* tx_packet = pta_tx_buffer_ring->elements[i].data;
		struct udp_hdr_total* hdr = (struct udp_hdr_total*) tx_packet;

		// Ethernet header
		hdr->eth.h_dest[0] = 0x38;
		hdr->eth.h_dest[1] = 0xf3;
		hdr->eth.h_dest[2] = 0xab;
		hdr->eth.h_dest[3] = 0x44;
		hdr->eth.h_dest[4] = 0xf7;
		hdr->eth.h_dest[5] = 0x9a;

		hdr->eth.h_source[0] = 0x00;
		hdr->eth.h_source[1] = 0x19;
		hdr->eth.h_source[2] = 0xb8;
		hdr->eth.h_source[3] = 0x09;
		hdr->eth.h_source[4] = 0x96;
		hdr->eth.h_source[5] = 0x60;

		hdr->eth.h_proto = htobe16(0x0800);

		// IP header
		hdr->ip.ip_hl = 0x5;
		hdr->ip.ip_v = 0x4;
		hdr->ip.ip_tos = 0x0;
		hdr->ip.ip_len = htobe16(1476);
		hdr->ip.ip_id = 0x0;
		hdr->ip.ip_off = htobe16(0x4000); // 0x0 in wiretrust from Nils?
		hdr->ip.ip_ttl = 64;
		hdr->ip.ip_p = 17; // UDP
		hdr->ip.ip_sum = 0x0;
		hdr->ip.ip_src.s_addr = htobe32(0xc0a8bcac); // 192.168.188.172
		hdr->ip.ip_dst.s_addr = htobe32(0xc0a8bc1d); // 192.168.188.29

		// UDP header
		hdr->udp.source = htobe16(6666);
		hdr->udp.dest = htobe16(6666);
		hdr->udp.len = htobe16(1456);
		hdr->udp.check = 0x0; // signals that no checksum was calculcated

		// Data
		uint8_t* data = tx_packet + ETH_HLEN + IP_HLEN + UDP_HLEN;
		memset(data, 0x42, 1448);

		// Benchmark Settings
		rx_bench_running = false;
		// remaining_rx_bench_duration = RX_BENCH_DURATION_S * 1000; // to ms
		rx_bench_bytes = 0;
		rx_bench_started = 0;
	}
}

/*
* ----------------------------------------------------------------------------
* Benchmark Throughput
* ----------------------------------------------------------------------------
*/
#ifdef BENCHMARK_THROUGHPUT
static uint64_t throughput(uint64_t current_freq) {
	uint64_t next_freq = current_freq;

	/*
	* ----------------------------------------------------------------------------
	* Receive Data
	* ----------------------------------------------------------------------------
	*/

	uint32_t consumed = 0;
	uint64_t consumed_bytes = 0;
	for(uint32_t i=0; i<DATARING_ELEMENTS_N; i++) {
		uint32_t index = (pta_rx_ring_read_pos + i) % DATARING_ELEMENTS_N;

		// not yet another element received
		if(pta_rx_ring_free[index]) {
			break;
		}

		// consume data
		pta_rx_ring_free[index] = true;
		consumed++;

		uint64_t payload_bytes = pta_rx_ring_size[index] - (ETH_HLEN + IP_HLEN + UDP_HLEN + ETH_FCS_LEN);

		#ifdef FLOODER
		uint8_t* data = pta_rx_buffer_ring->elements[index].data;
		if(data[128] == 0x42) {
			// check if this is a legit frame, only then we count it
			consumed_bytes = consumed_bytes + payload_bytes;
		} else {
			// do NOT count it, packet is from flooder!
		}
		#else
		consumed_bytes = consumed_bytes + payload_bytes;
		#endif
	}

	// INFO("Consumed: 0x%x\n", consumed);
	pta_rx_ring_read_pos = (pta_rx_ring_read_pos + consumed) % DATARING_ELEMENTS_N;

	// Time Accounting
	if(rx_bench_running) {

		// add bytes
		rx_bench_bytes = rx_bench_bytes + consumed_bytes;

		// consider passage of time
		uint64_t current = read_cntpct_el0();
		uint64_t passed = current - rx_bench_started;

		if(passed >= RX_BENCH_DURATION_S * read_cntfrq_el0()) {
			// benchmark is finished
			uint64_t bit = rx_bench_bytes * 8;
			uint64_t mbits = bit / 1000000 / RX_BENCH_DURATION_S;
			INFO("RX benchark finished. Bytes: 0x%llx, Time(s): %d, CIRCA (calc yourself) Mbit/s: %lld\n",
				rx_bench_bytes, RX_BENCH_DURATION_S, mbits);

			// reset benchmark
			rx_bench_running = false;
			rx_bench_started = 0;
			rx_bench_bytes = 0;
		}

	} else {

		if(consumed > 0) {
			INFO("RX benchmark started\n");
			rx_bench_running = true;
			rx_bench_started = read_cntpct_el0();
		}
	}

	/*
	* ----------------------------------------------------------------------------
	* Transmit Data
	* ----------------------------------------------------------------------------
	*/

	uint32_t sent = 0;
	if(tx_bench_running) {

		/*
		* We use the first ring as it is not used otherwise from the current
		* FEC Ethernet driver
		*/
		struct descring* tx_descring = state.shadow_tx_descrings[SECURE_TX_QUEUE];

		for(uint32_t i=0; i<DATARING_ELEMENTS_N; i++) {
			uint32_t index = (pta_tx_ring_pos + i) % DATARING_ELEMENTS_N;
			// INFO("Try to sent packet at index %d\n", index);

			struct bufdesc_ex* bd = &tx_descring->elements[index];

			dsb();
			uint16_t status = le16toh(bd->desc.cbd_sc);
			dsb();

			if(status & BD_ENET_TX_READY) {
				// INFO("BD at index %d is busy or still awaiting transmit\n", index);
				// buffer has not been transmitted or currently transmits
				break;
			}

			// packet has been transmitted, we can reuse the BD and send a new packet
			// INFO("BD at index %d is free\n", index);
			uint8_t* tx_packet = pta_tx_buffer_ring->elements[index].data;
			struct udp_hdr_total* hdr = (struct udp_hdr_total*) tx_packet;

			// Set IP identity, increase identity for next packet
			// Checksums are calculcated in hardware
			hdr->ip.ip_id = tx_ip_id++;

			// Flush out packet to RAM
			clean_dcache_range((uint64_t) tx_packet, TOTAL_TX_SIZE);

			// INFO("Prepare BD at index %d\n", index);
			bd->desc.cbd_bufaddr = htole32((uint32_t) ((uint64_t) tx_packet));
			bd->desc.cbd_datlen = le16toh(TOTAL_TX_SIZE);

			//note: the NW usually sets the BD_ENET_TX_INT bit in cbd_esc to enable interrupts
			//for this BD. Since we don't set the flag, the NW won't notice that we sent the
			//packet.

			//use hardware checksum computation
			bd->cbd_esc = le32toh(BD_ENET_PROT_CHECKSUM | BD_ENET_IP_CHECKSUM);
			bd->cbd_prot = le32toh(0);
			bd->cbd_bdu = le32toh(0);

			// update status flag
			status = status | BD_ENET_TX_READY | BD_ENET_TX_LAST | BD_ENET_TC;

			/* INFO("Sent packet: \n");
			uint32_t j;
			for(j=0; j<TOTAL_TX_SIZE; j++) {
				printf("%02x ", tx_packet[j]);
			}
			printf("\n"); */

			// INFO("Transmit ownership of BD at index %d\n", index);
			linux_dma_wmb();
			bd->desc.cbd_sc = htole16(status);
			linux_wmb();

			//writing to this register indicates that TX ring 2 has been updated
			// INFO("Tell NIC new data is available\n");
			mmio_write_32(IMX_NIC_BASE + ENET_TDAR2, 0);

			// send data
			sent++;
		}

		pta_tx_ring_pos = (pta_tx_ring_pos + sent) % DATARING_ELEMENTS_N;
		// INFO("Update TX ring pos to %d\n", pta_tx_ring_pos);
	}

	/*
	* ----------------------------------------------------------------------------
	* Set the next frequency
	* ----------------------------------------------------------------------------
	*/

	// Test if we need to change the frequency
	if(consumed <= 0.2 * DATARING_ELEMENTS_N && sent <= 0.2 * DATARING_ELEMENTS_N) {
		// INFO("decrease freq\n");

		// do not get lower than threshold
		if(current_freq - 10 < 20) {
			next_freq = current_freq;
		} else {
			next_freq = current_freq - 10;
		}

	} else if(consumed >= 0.8 * DATARING_ELEMENTS_N || sent >= 0.8 * DATARING_ELEMENTS_N) {
		// INFO("increase freq\n");

		// do not get higher than threshold
		if(current_freq + 10 > 170) {
			next_freq = current_freq;
		} else {
			next_freq = current_freq + 10;
		}
	}

	return next_freq;
}
#endif

/*
* ----------------------------------------------------------------------------
* Benchmark Latency
* ----------------------------------------------------------------------------
*/
#ifndef BENCHMARK_THROUGHPUT
#ifdef BENCHMARK_LATENCY_2
static uint64_t latency(uint64_t current_freq) {
	uint64_t next_freq = current_freq;

	if(tx_bench_running) {
		// Only send one packet if module is loaded
		tx_bench_running = false;

		/* -------------------------------------------------------------------------
		* Transmit Frame
		* ------------------------------------------------------------------------*/

		/*
		* We use the first ring as it is not used otherwise from the current
		* FEC Ethernet driver
		*/
		struct descring* tx_descring = state.shadow_tx_descrings[SECURE_TX_QUEUE];
		uint32_t index = pta_tx_ring_pos;
		struct bufdesc_ex* bd = &tx_descring->elements[index];

		dsb();
		uint16_t status = le16toh(bd->desc.cbd_sc);
		dsb();

		if(status & BD_ENET_TX_READY) {
			INFO("BD at index %d is busy or still awaiting transmit\n", index);
			// buffer has not been transmitted or currently transmits
			return next_freq;
		}

		// packet has been transmitted, we can reuse the BD and send a new packet
		// INFO("BD at index %d is free\n", index);
		uint8_t* tx_packet = pta_tx_buffer_ring->elements[index].data;
		struct udp_hdr_total* hdr = (struct udp_hdr_total*) tx_packet;

		// Set IP identity, increase identity for next packet
		// Checksums are calculcated in hardware
		hdr->ip.ip_id = tx_ip_id++;

		// Flush out packet to RAM
		clean_dcache_range((uint64_t) tx_packet, TOTAL_TX_SIZE);

		// INFO("Prepare BD at index %d\n", index);
		bd->desc.cbd_bufaddr = htole32((uint32_t) ((uint64_t) tx_packet));
		bd->desc.cbd_datlen = le16toh(TOTAL_TX_SIZE);

		//note: the NW usually sets the BD_ENET_TX_INT bit in cbd_esc to enable interrupts
		//for this BD. Since we don't set the flag, the NW won't notice that we sent the
		//packet.

		//use hardware checksum computation
		bd->cbd_esc = le32toh(BD_ENET_PROT_CHECKSUM | BD_ENET_IP_CHECKSUM);
		bd->cbd_prot = le32toh(0);
		bd->cbd_bdu = le32toh(0);

		// update status flag
		status = status | BD_ENET_TX_READY | BD_ENET_TX_LAST | BD_ENET_TC;

		/* INFO("Sent packet: \n");
		uint32_t j;
		for(j=0; j<TOTAL_TX_SIZE; j++) {
			printf("%02x ", tx_packet[j]);
		}
		printf("\n"); */

		// INFO("Transmit ownership of BD at index %d\n", index);
		linux_dma_wmb();
		bd->desc.cbd_sc = htole16(status);
		linux_wmb();

		//writing to this register indicates that TX ring 2 has been updated
		// INFO("Tell NIC new data is available\n");
		mmio_write_32(IMX_NIC_BASE + ENET_TDAR2, 0);

		rx_bench_started = read_cntpct_el0();

		pta_tx_ring_pos = (pta_tx_ring_pos + 1) % DATARING_ELEMENTS_N;
	}

	/* -------------------------------------------------------------------------
	* Receive Frame
	* -------------------------------------------------------------------------*/

	uint32_t consumed = 0;
	for(uint32_t i=0; i<DATARING_ELEMENTS_N; i++) {
		uint32_t index = (pta_rx_ring_read_pos + i) % DATARING_ELEMENTS_N;

		// not yet another element received
		if(pta_rx_ring_free[index]) {
			break;
		}

		// consume data
		pta_rx_ring_free[index] = true;
		consumed++;

		// measure RTT
		uint64_t current = read_cntpct_el0();
		uint64_t passed = current - rx_bench_started;
		uint64_t freq = read_cntfrq_el0();

		// output:
		INFO("RTT: Passed: 0x%llx, Frequency: 0x%llx\n", passed, freq);

		rx_bench_started = 0;
	}

	pta_rx_ring_read_pos = (pta_rx_ring_read_pos + consumed) % DATARING_ELEMENTS_N;

	return next_freq;
}
#else
static uint64_t latency(uint64_t current_freq) {
	uint64_t next_freq = current_freq;

	/*
	* ----------------------------------------------------------------------------
	* Receive Data
	* ----------------------------------------------------------------------------
	*/

	uint32_t consumed = 0;
	uint64_t consumed_bytes = 0;
	for(uint32_t i=0; i<DATARING_ELEMENTS_N; i++) {
		uint32_t index = (pta_rx_ring_read_pos + i) % DATARING_ELEMENTS_N;

		// not yet another element received
		if(pta_rx_ring_free[index]) {
			break;
		}

		/*
		* --------------------------------------------------------------------------
		* Transmit Data Begin
		* --------------------------------------------------------------------------
		*/

		/*
		* We use the first ring as it is not used otherwise from the current
		* FEC Ethernet driver
		*/
		struct descring* tx_descring = state.shadow_tx_descrings[SECURE_TX_QUEUE];
		struct bufdesc_ex* bd = &tx_descring->elements[pta_tx_ring_pos];

		dsb();
		uint16_t status = le16toh(bd->desc.cbd_sc);
		dsb();

		if(status & BD_ENET_TX_READY) {
			// INFO("BD at index %d is busy or still awaiting transmit\n", index);
			// buffer has not been transmitted or currently transmits
			break;
		}

		// packet has been transmitted, we can reuse the BD and send a new packet
		// INFO("BD at index %d is free\n", index);
		uint8_t* tx_packet = pta_tx_buffer_ring->elements[index].data;
		struct udp_hdr_total* hdr = (struct udp_hdr_total*) tx_packet;

		// Set IP identity, increase identity for next packet
		// Checksums are calculcated in hardware
		hdr->ip.ip_id = tx_ip_id++;

		// Flush out packet to RAM
		clean_dcache_range((uint64_t) tx_packet, TOTAL_TX_SIZE);

		// INFO("Prepare BD at index %d\n", index);
		bd->desc.cbd_bufaddr = htole32((uint32_t) ((uint64_t) tx_packet));
		bd->desc.cbd_datlen = le16toh(TOTAL_TX_SIZE);

		//note: the NW usually sets the BD_ENET_TX_INT bit in cbd_esc to enable interrupts
		//for this BD. Since we don't set the flag, the NW won't notice that we sent the
		//packet.

		//use hardware checksum computation
		bd->cbd_esc = le32toh(BD_ENET_PROT_CHECKSUM | BD_ENET_IP_CHECKSUM);
		bd->cbd_prot = le32toh(0);
		bd->cbd_bdu = le32toh(0);

		// update status flag
		status = status | BD_ENET_TX_READY | BD_ENET_TX_LAST | BD_ENET_TC;

		/* INFO("Sent packet: \n");
		uint32_t j;
		for(j=0; j<TOTAL_TX_SIZE; j++) {
			printf("%02x ", tx_packet[j]);
		}
		printf("\n"); */

		// INFO("Transmit ownership of BD at index %d\n", index);
		linux_dma_wmb();
		bd->desc.cbd_sc = htole16(status);
		linux_wmb();

		//writing to this register indicates that TX ring 2 has been updated
		// INFO("Tell NIC new data is available\n");
		mmio_write_32(IMX_NIC_BASE + ENET_TDAR2, 0);

		pta_tx_ring_pos = (pta_tx_ring_pos + 1) % DATARING_ELEMENTS_N;
		// INFO("Update TX ring pos to %d\n", pta_tx_ring_pos);

		/*
		* --------------------------------------------------------------------------
		* Transmit Data End
		* --------------------------------------------------------------------------
		*/

		// consume data
		pta_rx_ring_free[index] = true;
		consumed++;

		uint64_t payload_bytes = pta_rx_ring_size[index] - (ETH_HLEN + IP_HLEN + UDP_HLEN + ETH_FCS_LEN);
		consumed_bytes = consumed_bytes + payload_bytes;
	}

	// INFO("Consumed: 0x%x\n", consumed);
	pta_rx_ring_read_pos = (pta_rx_ring_read_pos + consumed) % DATARING_ELEMENTS_N;

	return next_freq;
}
#endif
#endif

/*
* Here, we emulate a PTA that wants to transmit and receive data.
*/
uint64_t pta_tick(uint64_t current_freq) {
	#ifdef BENCHMARK_THROUGHPUT
	return throughput(current_freq);
	#else
	return latency(current_freq);
	#endif
}
