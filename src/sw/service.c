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

#include "service.h"
#include "nic_mmio.h"
#include "state.h"
#include "descring.h"
#include "dataring.h"
#include "timer.h"
#include "app/app.h"
#include "nic_tx_sync.h"
#include "nic_tx_ack.h"
#include "nic_rx_sync.h"
#include "nic_rx_ack.h"
#include "checks.h"
#include "app/app.h"

#include <lib/smccc.h>
#include <lib/spinlock.h>
#include <common/runtime_svc.h>
#include <lib/mmio.h>
#include <arch_helpers.h>
#include <drivers/arm/tzc380.h>


/* -----------------------------------------------------------------------------
* Global variables
* --------------------------------------------------------------------------- */

// The spinlock to serialize access to the EL3 ATF service.
spinlock_t prot_lock;

/* -----------------------------------------------------------------------------
* Constructor
* --------------------------------------------------------------------------- */

static int32_t service_svc_setup(void)
{
  spin_lock(&prot_lock);

  // INFO("Initialize NIC (V: 1.12)\n");

  // shadow_tx_current
	for(uint32_t i=0; i<NUMBER_TX_QUEUES; i++) {
		state.shadow_tx_current[i] = 0x0;
	}

  /* Initialize the state of the framework */
  // nw_tx_descrings
	for(uint32_t i=0; i<NUMBER_RX_QUEUES; i++) {
		state.nw_rx_descrings[i] = NULL;
	}

  // nw_rx_descrings
	for(uint32_t i=0; i<NUMBER_TX_QUEUES; i++) {
		state.nw_tx_descrings[i] = NULL;
	}

	// Initialize descrings so that they point to the reserved memory
  // shadow_rx_descrings
	state.shadow_rx_descrings[0] = (struct descring*) ((uint64_t) SHADOW_DESCRINGS_BASE + RECEIVE_0_OFFSET);
	state.shadow_rx_descrings[1] = (struct descring*) ((uint64_t) SHADOW_DESCRINGS_BASE + RECEIVE_1_OFFSET);
	state.shadow_rx_descrings[2] = (struct descring*) ((uint64_t) SHADOW_DESCRINGS_BASE + RECEIVE_2_OFFSET);
  // shadow_tx_descrings
	state.shadow_tx_descrings[0] = (struct descring*) ((uint64_t) SHADOW_DESCRINGS_BASE + TRANSMIT_0_OFFSET);
	state.shadow_tx_descrings[1] = (struct descring*) ((uint64_t) SHADOW_DESCRINGS_BASE + TRANSMIT_1_OFFSET);
	state.shadow_tx_descrings[2] = (struct descring*) ((uint64_t) SHADOW_DESCRINGS_BASE + TRANSMIT_2_OFFSET);

	// Initialize the datarings so they point to the reserved memory
  // rx_datarings
	state.rx_datarings[0] = (struct dataring*) ((uint64_t) DATARING_BASE + DATARING_RECEIVE_0_OFFSET);
	state.rx_datarings[1] = (struct dataring*) ((uint64_t) DATARING_BASE + DATARING_RECEIVE_1_OFFSET);
	state.rx_datarings[2] = (struct dataring*) ((uint64_t) DATARING_BASE + DATARING_RECEIVE_2_OFFSET);

  // INFO("Initializing the PTA\n");
  pta_init();

	spin_unlock(&prot_lock);
	return 0;
}

/* -----------------------------------------------------------------------------
* Helper
* --------------------------------------------------------------------------- */

/*
* We assign the NIC to the SW during the runtime of the kernel (during boot).
* This way we do not need to adjust U-Boot as well, which also configures the
* NIC, and, thus, would trap. This is not secure for a productive setup.
*/
static void prot_nic() {
  // INFO("Restricting access to NIC\n");
  dsb();
  mmio_write_32(IMX_CSU_BASE + 47 * 4, 0x00ff0033); // disallow access from NW
  // mmio_write_32(IMX_CSU_BASE + 47 * 4, 0x00ff00ff); // allow access from NW
  dsb();
  // INFO("Restricting access to NIC done\n");

  // NOTICE("Starting to configure the ring buffer memory\n");
  tzc380_init(IMX_TZASC_BASE);

  // NOTICE("Starting to configure the ring buffer for the buffer descriptors\n");
  // Ring Buffer is from 0xA0200000 to 0xA0400000.
  tzc380_configure_region(2, 0x60200000, TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_2M) |
        TZC_ATTR_REGION_EN_MASK | TZC_ATTR_SP_S_RW);
  // NOTICE("Finished to configure the ring buffer for the buffer descriptors\n");

  // Data Ring is from 0xA0400000 to 0xA0C00000.
  // NOTICE("Starting to configure the data ring memory\n");
  tzc380_configure_region(3, 0x60400000, TZC_ATTR_REGION_SIZE(TZC_REGION_SIZE_8M) |
        TZC_ATTR_REGION_EN_MASK | TZC_ATTR_SP_S_RW);
  // NOTICE("Finished to configure the data ring memory\n");

  dsb();
}


/* -----------------------------------------------------------------------------
* Global SMC handler
* --------------------------------------------------------------------------- */

/*
* This function is called whenever an SMC for our service is executed. It is
* the main entry point.
*/
static uintptr_t service_svc_smc_handler(uint32_t smc_fid,
			     u_register_t x1,
			     u_register_t x2,
			     u_register_t x3,
			     u_register_t x4,
			     void *cookie,
			     void *handle,
			     u_register_t flags)
{
  uint32_t oen = GET_SMC_OEN(smc_fid);
	switch (oen) {
  case FUNCID_PROT_NIC_RAW: {
      spin_lock(&prot_lock);
			/*
			* We assign the NIC to the SW during the runtime of the kernel (during boot).
			* This way we do not need to adjust U-Boot as well, which also configures the
			* NIC, and, thus, would trap. This is not secure for a productive setup.
			*/
			prot_nic();

      // INFO("Setting the timer\n");
      my_timer_init();

      spin_unlock(&prot_lock);
    	SMC_RET0(handle);
  } case FUNCID_WRITE_32_RAW: {
      spin_lock(&prot_lock);
			uint32_t val = (uint32_t) x1;
      uint64_t address = (uint64_t) x2;
      nic_write32(val, address);
      spin_unlock(&prot_lock);
    	SMC_RET0(handle);
  } case FUNCID_READ_32_RAW: {
      spin_lock(&prot_lock);
      uint64_t address = (uint64_t) x1;
      uint32_t val = nic_read32(address);
      spin_unlock(&prot_lock);
    	SMC_RET1(handle, (uint64_t) val);
  } case FUNCID_TX_SYNC_RAW: {
      spin_lock(&prot_lock);
			uint32_t queue = (uint32_t) x1;
			uint32_t current = (uint32_t) x2;
      nic_tx_sync(queue, current);
      spin_unlock(&prot_lock);
    	SMC_RET0(handle);
  } case FUNCID_TX_ACK_RAW: {
      spin_lock(&prot_lock);
			uint32_t queue = (uint32_t) x1;
			uint32_t pending_tx_position = (uint32_t) x2;
      nic_tx_ack(queue, pending_tx_position);
      spin_unlock(&prot_lock);
    	SMC_RET0(handle);
  } case FUNCID_RX_SYNC_RAW: {
      spin_lock(&prot_lock);
			uint32_t queue = (uint32_t) x1;
			uint32_t current = (uint32_t) x2;
			uint32_t budget = (uint32_t) x3;
      nic_rx_sync(queue, current, budget);
      spin_unlock(&prot_lock);
    	SMC_RET0(handle);
  } case FUNCID_RX_ACK_RAW: {
      spin_lock(&prot_lock);
			uint32_t queue = (uint32_t) x1;
			uint32_t current = (uint32_t) x2;
			uint32_t received = (uint32_t) x3;
      nic_rx_ack(queue, current, received);
      spin_unlock(&prot_lock);
    	SMC_RET0(handle);
  } case FUNCID_TX_BENCH_START_RAW: {
      spin_lock(&prot_lock);
      tx_bench_running = true;
      spin_unlock(&prot_lock);
    	SMC_RET0(handle);
  } case FUNCID_TX_BENCH_STOP_RAW: {
      spin_lock(&prot_lock);
      tx_bench_running = false;
      spin_unlock(&prot_lock);
    	SMC_RET0(handle);
  } default: {
      // ERROR("Unknown OEN: %d\n", oen);
    	SMC_RET0(handle);
  } }
}

DECLARE_RT_SVC(
		prot_svc,
		OEN_PROT_START,
		OEN_PROT_END,
		SMC_TYPE_FAST,
		service_svc_setup,
		service_svc_smc_handler
);
