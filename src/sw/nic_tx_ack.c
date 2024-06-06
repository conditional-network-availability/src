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

#include "nic_tx_ack.h"
#include "nic_tx_common.h"
#include "descring.h"
#include "dataring.h"
#include "asm/memcpy.h"
#include "checks.h"
#include "state.h"

/* -----------------------------------------------------------------------------
* TX_ACK
* --------------------------------------------------------------------------- */

void tx_ack_bd_handler(struct bufdesc_ex* dest, struct bufdesc_ex* origin) {
  uint32_t data = le32toh(dest->desc.cbd_bufaddr);
  copy_desc(dest, origin, data);
}


void tx_ack_iterator(uint32_t pending_tx_position, uint32_t shadow_tx_current,
    struct descring* nw_ring, struct descring* shadow_ring) {

  // If current values match, the shadow buffer is most up-to-date.
  if(pending_tx_position == shadow_tx_current) // {
    return;
  // }

  // We need to synchronize the ring buffer with the shadow ring
	// buffer. We need to watch out for a wrap in the ring.
  uint32_t n;
  if(pending_tx_position < shadow_tx_current) {
    // no wraparound
    n = shadow_tx_current - pending_tx_position;
  } else {
    // wraparound
    n = (DESCRING_ELEMENTS_N - pending_tx_position) + shadow_tx_current;
  }

  for(uint32_t i=0; i<n; i++) {
    uint32_t index = (pending_tx_position + i) % DESCRING_ELEMENTS_N;
    // We know that index is valid, i.e., in [0, DESCRING_ELEMENTS_N-1] here

    struct bufdesc_ex* origin = &shadow_ring->elements[index];

  	// We want to return false if we want to abort the loop process.
  	// We do only want to copy buffer descriptors that are already transmitted.
  	// This also means what we can free the data for them as they are already
  	// transmitted. For the others, we still need the data and cannot comsume
  	// the data ring entry.
  	// TX_READY means ready to transmit = not yet transmitted
    uint16_t status = le16toh(origin->desc.cbd_sc);
    if(status & BD_ENET_TX_READY) // {
      break;
    // }

    struct bufdesc_ex* dest = &nw_ring->elements[index];

    tx_ack_bd_handler(dest, origin);
  }
}


void nic_tx_ack(uint32_t queue, uint32_t pending_tx_position) {
  if(!check_tx_queue(queue) || !check_pos(pending_tx_position)) // {
    // ERROR("Invalid TX queue: %d\n", queue);
    return;
  // }

  if(!is_nw_ram_region((uint64_t) state.nw_tx_descrings[queue], DESCRING_SIZE_BYTES)) // {
    // ERROR("NW ring not correct: Not a NW RAM region: %llx, %d\n",
    //  (uint64_t) state.nw_tx_descrings[queue], DESCRING_SIZE_BYTES);
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

  tx_ack_iterator(pending_tx_position, shadow_tx_current, nw_ring,
      shadow_ring);
}
