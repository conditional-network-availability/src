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

#include "nic_tx_sync.h"
#include "nic_tx_common.h"
#include "nic_common.h"
#include "dataring.h"
#include "descring.h"
#include "asm/memcpy.h"
#include "checks.h"
#include "state.h"

/* -----------------------------------------------------------------------------
* TX_SYNC
* --------------------------------------------------------------------------- */

void tx_sync_bd_handler(struct bufdesc_ex* dest, struct bufdesc_ex* origin,
    uint32_t index) {

	// Copy buffer descriptor to shadow ring to prevent concurrent updates while
	// we do our analysis: Important: Do NOT yet transmit ownership of the
  // descriptor to the NIC to prevent that the NIC transmits unchecked packages.
	uint16_t prev_status = copy_desc_wo_ownership(dest, origin);
  if(!(prev_status & BD_ENET_TX_READY)) // {
    // ERROR("tx_sync_bd_handler: BD not ready\n");
    return;
  // }

	uint64_t size = (uint64_t) le16toh(dest->desc.cbd_datlen);
  uint8_t* origData = (uint8_t*) ((uint64_t) (le32toh(dest->desc.cbd_bufaddr)));
	uint16_t status = le16toh(dest->desc.cbd_sc);

  // In conditional availability: Do NOT copy to data ring, since we do NOT
  // need to filter the traffic. However, we need to ensure that buffer
  // descriptor rings do not point to secure world memory (since we assingn
  // NIC to SW.). Thus, we need to check the buffer descriptor rings and keep
  // a shadow copy of them.

  if(!is_nw_ram_region((uint64_t) origData, size)) // {
    // ERROR("tx_sync_bd_handler: Not a NW RAM region: %p, %lld\n", origData, size);
    return;
  // }

	// Check if we are about to put an BD to the BD ring that can contain a
	// header. If the packet could contain a header, we need to copy the data
	// (incl. the header) to the data ring.
	// If not, we currently refrain from copying the data.
	uint8_t* data = origData;

  // sync to RAM, i.e. CLEAN, i.e. ensure that the data lands in RAM.
  clean_dcache_range((uint64_t) data, size);

  // We need to enforce that WRAP is set if we are last in the ring.
  // Otherwise, a malicious NW might try to overrun the shadow ring buffers,
  // potentially causing harm.
  if(index == DESCRING_ELEMENTS_N - 1 && (status & BD_SC_WRAP) == 0x0) // {
    // ERROR("tx_sync_bd_handler: Last descriptor does not WRAP\n");
    dest->desc.cbd_sc = status | BD_SC_WRAP;
  // }

	// Transmit ownership
	transmit_ownership(dest);
}


void tx_sync_iterator(uint32_t current, uint32_t shadow_tx_current,
    struct descring* nw_ring, struct descring* shadow_ring) {

  // If current values match, the shadow buffer is most up-to-date.
  if(current == shadow_tx_current) // {
    return;
  // }

  // We need to synchronize the ring buffer with the shadow ring
	// buffer. We need to watch out for a wrap in the ring.
  uint32_t n;
  if(current > shadow_tx_current) {
    // no wraparound
    n = current - shadow_tx_current;
  } else {
    // wraparound
    n = (DESCRING_ELEMENTS_N - shadow_tx_current) + current;
  }

  for(uint32_t i=0; i<n; i++) {
    uint32_t index = (shadow_tx_current + i) % DESCRING_ELEMENTS_N;
    // We know that index is valid, i.e., in [0, DESCRING_ELEMENTS_N-1] here

    struct bufdesc_ex* origin = &nw_ring->elements[index];

    uint16_t status = le16toh(origin->desc.cbd_sc);
    if(!(status & BD_ENET_TX_READY)) // {
      break;
    // }

    struct bufdesc_ex* dest = &shadow_ring->elements[index];
    tx_sync_bd_handler(dest, origin, index);
  }
}


void nic_tx_sync(uint32_t queue, uint32_t current) {
  if(!check_tx_queue(queue) || !check_pos(current)) // {
    // ERROR("Invalid TX queue: %d\n", queue);
    return;
  // }

  if(!is_nw_ram_region((uint64_t) state.nw_tx_descrings[queue], DESCRING_SIZE_BYTES)) // {
    // ERROR("NW ring not correct: Not a NW RAM region: %llx, %d\n",
    //   (uint64_t) state.nw_tx_descrings[queue], DESCRING_SIZE_BYTES);
    return;
  // }
  struct descring* nw_ring = state.nw_tx_descrings[queue];

  struct descring* shadow_ring = state.shadow_tx_descrings[queue];
  if(!shadow_ring) // {
    // ERROR("Shadow ring is not initialized\n");
    return;
  // }

  uint32_t shadow_tx_current = state.shadow_tx_current[queue];
  if(!check_pos(shadow_tx_current)) // {
    // ERROR("Invalid shadow_tx_current position: %d\n", shadow_tx_current);
    return;
  // }

  tx_sync_iterator(current, shadow_tx_current, nw_ring, shadow_ring);

  state.shadow_tx_current[queue] = current;
}
