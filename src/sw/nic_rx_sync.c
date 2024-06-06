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

#include <stdint.h>
#include <lib/mmio.h>
#include <endian.h>
#include <arch_helpers.h>
#include <platform_def.h>

#include "nic_rx_sync.h"
#include "nic_rx_common.h"
#include "nic_common.h"
#include "asm/memcpy.h"
#include "asm/memset.h"
#include "checks.h"
#include "descring.h"
#include "dataring.h"
#include "state.h"
#include "selector.h"

/* -----------------------------------------------------------------------------
* RX_SYNC
* --------------------------------------------------------------------------- */

void rx_sync_frame_handler(uint8_t* dstData, uint8_t* originData,
    uint16_t originSize) {

  if(!is_nw_ram_region((uint64_t) dstData, FEC_MAX_FRAME_LEN)) // {
    // ERROR("rx_sync_frame_handler: Not a NW RAM region: %p, %d\n", dstData, originSize);
    return;
  // }

  inv_dcache_range((uint64_t) originData, originSize);

  // The selector also consumes the packet in the SW.
  bool was_sw = selector_run(originData, originSize);
  if(was_sw) {
    // clear out frame in NW to inject nullframe
    destroy(dstData, FEC_MAX_FRAME_LEN);
  } else {
    // Copy the legit packet to NW.
    memcpy_aarch64(dstData, originData, originSize);
  }

  dsb();
}


void rx_sync_bd_handler(struct bufdesc_ex* dest, struct bufdesc_ex* origin,
    uint32_t index, struct element* element) {

  uint32_t originData = le32toh(origin->desc.cbd_bufaddr);
	uint16_t originSize = le16toh(origin->desc.cbd_datlen);
  uint32_t dstData = le32toh(dest->desc.cbd_bufaddr);
	uint16_t status = le16toh(origin->desc.cbd_sc);

  // Check source frame for validity and correct length.
  // Those are the preconditions for the frame_handler.
  if((uint8_t*) ((uint64_t) originData) != ((uint8_t*) element)) // {
    // ERROR("rx_sync_bd_handler: Not a shadow ring region: %llx\n", (uint64_t) originData);
    return;
  // }

  if(!check_rx_packet_size(originSize)) // {
    // ERROR("rx_sync_bd_handler: Not a valid length: %x\n", originSize);
    return;
  // }

	rx_sync_frame_handler((uint8_t*)((uint64_t) dstData), (uint8_t*) ((uint64_t) originData), originSize);

  // Verify that BD wraps are correctly handled
  if(index == DESCRING_ELEMENTS_N - 1 && (status & BD_SC_WRAP) == 0x0) // {
    // ERROR("rx_sync_bd_handler: Last descriptor does not WRAP\n");
    origin->desc.cbd_sc = status | BD_SC_WRAP;
  // }

  // After the data, we can copy over the descriptor, issuing the NW address
  // as a new address for the buffer descriptor.
  copy_desc(dest, origin, dstData);
}


void rx_sync_iterator(uint32_t current, uint32_t budget, struct descring* nw_ring,
    struct descring* shadow_ring, struct dataring* shadow_data) {

  for(uint32_t i=0; i<budget; i++) {
    uint32_t index = (current + i) % DESCRING_ELEMENTS_N;
    // We know that index is valid, i.e., in [0, DESCRING_ELEMENTS_N-1] here
  	struct bufdesc_ex* origin = &shadow_ring->elements[index];

    // We do not include endianess in our proof since it influences its duration
    // exponentially and prevents that we can reason with higher data types.
    uint16_t status = le16toh(origin->desc.cbd_sc);
    if(status & BD_ENET_RX_EMPTY) // {
      break;
    // }

    struct bufdesc_ex* dest = &nw_ring->elements[index];
    struct element* element = &shadow_data->elements[index];
    rx_sync_bd_handler(dest, origin, index, element);
  }
}


void nic_rx_sync(uint32_t queue, uint32_t current, uint32_t budget) {
  if(!check_rx_queue(queue) || !check_pos(current) || !check_budget(budget)) // {
    // ERROR("Invalid RX queue: %d\n", queue);
    return;
  // }

  if(!is_nw_ram_region((uint64_t) state.nw_rx_descrings[queue], DESCRING_SIZE_BYTES)) // {
    // ERROR("NW ring not correct: Not a NW RAM region: %llx, %d\n",
    //  (uint64_t) state.nw_rx_descrings[queue], DESCRING_SIZE_BYTES);
    return;
  // }
  struct descring* nw_ring = state.nw_rx_descrings[queue];

  struct descring* shadow_ring = state.shadow_rx_descrings[queue];
  if(!shadow_ring) // {
    // ERROR("Shadow ring is not initialized\n");
    return;
  // }

  struct dataring* shadow_data = state.rx_datarings[queue];
  if(!shadow_data) // {
    // ERROR("Shadow data ring is not initialized\n");
    return;
  // }

  rx_sync_iterator(current, budget, nw_ring, shadow_ring, shadow_data);
}
