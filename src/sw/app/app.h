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

#ifndef PTA_H
#define PTA_H

#include <stdint.h>
#include <assert.h>
#include <common/debug.h>
#include <stdbool.h>

#include "../dataring.h"

/* -----------------------------------------------------------------------------
* Global State
* --------------------------------------------------------------------------- */

#define SECURE_TX_QUEUE 2

// dataring with 512 elements a 2048 bytes
#define PTA_RX_BUFFER_BASE 0xA0C00000
extern struct dataring* pta_rx_buffer_ring;
extern bool pta_rx_ring_free[DATARING_ELEMENTS_N]; // element is free
extern uint32_t pta_rx_ring_size[DATARING_ELEMENTS_N]; // element is free
extern uint32_t pta_rx_ring_insert_pos; // pos to insert next data
extern uint32_t pta_rx_ring_read_pos; // pos to read next data

// dataring with 512 elements a 2048 bytes
#define PTA_TX_BUFFER_BASE 0xA0D00000
extern uint32_t pta_tx_ring_pos;

extern bool tx_bench_running;

/* -----------------------------------------------------------------------------
* Functions
* --------------------------------------------------------------------------- */

void pta_init(void);

// returns next freq, based on read_cntfrq_el0
uint64_t pta_tick(uint64_t current_freq);

#endif
