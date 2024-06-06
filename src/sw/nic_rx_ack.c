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
#include <endian.h>

#include "nic_rx_ack.h"
#include "nic_rx_common.h"
#include "descring.h"
#include "dataring.h"
#include "asm/memcpy.h"
#include "checks.h"
#include "state.h"

/* -----------------------------------------------------------------------------
* RX_ACK
* --------------------------------------------------------------------------- */

void rx_ack_bd_handler(struct bufdesc_ex* dest, struct bufdesc_ex* origin,
    uint32_t index, struct element* element) {

	uint16_t status = le16toh(origin->desc.cbd_sc);

  if(((uint64_t) element) > 0xFFFFFFFF) // {
    // ERROR("Address too large\n");
    return;
  // }
  uint32_t newData = (uint32_t) ((uint64_t) element);

  // Verify that BD wraps are correctly handled
  if(index == DESCRING_ELEMENTS_N - 1 && (status & BD_SC_WRAP) == 0x0) // {
    // ERROR("rx_ack_bd_handler: Last descriptor does not WRAP\n");
    origin->desc.cbd_sc = status | BD_SC_WRAP;
  // }

  copy_desc(dest, origin, newData);
}


void rx_ack_iterator(uint32_t current, uint32_t received, struct descring* nw_ring,
    struct descring* shadow_ring, struct dataring* shadow_data) {

  for(uint32_t i=0; i<received; i++) {
    uint32_t index = (current + i) % DESCRING_ELEMENTS_N;
    // We know that index is valid, i.e., in [0, DESCRING_ELEMENTS_N-1] here

  	struct bufdesc_ex* origin = &nw_ring->elements[index];

  	uint16_t status = le16toh(origin->desc.cbd_sc);
  	if(!(status & BD_ENET_TX_READY)) // {
      break;
  	// }

    struct bufdesc_ex* dest = &shadow_ring->elements[index];
    struct element* element = &shadow_data->elements[index];

    rx_ack_bd_handler(dest, origin, index, element);
  }
}


void nic_rx_ack(uint32_t queue, uint32_t current, uint32_t received) {
  if(!check_rx_queue(queue) || !check_pos(current) || !check_budget(received)) // {
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

  rx_ack_iterator(current, received, nw_ring, shadow_ring, shadow_data);
}
