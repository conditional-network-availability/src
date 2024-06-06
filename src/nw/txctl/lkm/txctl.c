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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/arm-smccc.h>

// ----------------------------------------------------------------------------
// SMC Protocol Definition
// ----------------------------------------------------------------------------

#define FUNCID_OEN_SHIFT		UL(24)
#define FUNCID_OEN_MASK			UL(0x3f)
#define FUNCID_OEN_WIDTH		UL(6)

#define SMC_TYPE_FAST			UL(1)
#define SMC_TYPE_YIELD			UL(0)

#define FUNCID_TYPE_SHIFT		UL(31)
#define FUNCID_TYPE_MASK		UL(0x1)
#define FUNCID_TYPE_WIDTH		UL(1)

#define FUNCID_TYPE (SMC_TYPE_FAST << FUNCID_TYPE_SHIFT)

#define FUNCID_TX_BENCH_START_RAW UL(26)
#define FUNCID_TX_BENCH_START_OEN (FUNCID_TX_BENCH_START_RAW << FUNCID_OEN_SHIFT)
#define FUNCID_TX_BENCH_START (FUNCID_TX_BENCH_START_OEN | FUNCID_TYPE)

#define FUNCID_TX_BENCH_STOP_RAW UL(27)
#define FUNCID_TX_BENCH_STOP_OEN (FUNCID_TX_BENCH_STOP_RAW << FUNCID_OEN_SHIFT)
#define FUNCID_TX_BENCH_STOP (FUNCID_TX_BENCH_STOP_OEN | FUNCID_TYPE)

// ----------------------------------------------------------------------------
// SMC Wrappers
// ----------------------------------------------------------------------------

__attribute__((always_inline))
static inline void tx_bench_start(void) {
	struct arm_smccc_res res;
	printk(KERN_ALERT "DEBUG: Passed %s %d \n",__FUNCTION__,__LINE__);
	arm_smccc_smc(
		FUNCID_TX_BENCH_START, // a0
		0, // a1
		0, // a2
		0, // a3
		0, // a4
		0, // a5
		0, // a6
		0, // a7
		&res // res
	);
	printk(KERN_ALERT "DEBUG: Passed %s %d \n",__FUNCTION__,__LINE__);
}

__attribute__((always_inline))
static inline void tx_bench_stop(void) {
	struct arm_smccc_res res;
	printk(KERN_ALERT "DEBUG: Passed %s %d \n",__FUNCTION__,__LINE__);
	arm_smccc_smc(
		FUNCID_TX_BENCH_STOP, // a0
		0, // a1
		0, // a2
		0, // a3
		0, // a4
		0, // a5
		0, // a6
		0, // a7
		&res // res
	);
	printk(KERN_ALERT "DEBUG: Passed %s %d \n",__FUNCTION__,__LINE__);
}

// ----------------------------------------------------------------------------
// Init Function of the LKM (Constructor)
// ----------------------------------------------------------------------------

static int txctl_init(void) {
    printk(KERN_DEBUG "Starting TX benchark\n");
    tx_bench_start();
    return 0;
}

// ----------------------------------------------------------------------------
// Exit Function of the LKM (Destructor)
// ----------------------------------------------------------------------------

static void txctl_exit(void) {
    printk(KERN_DEBUG "Stopping TX benchmark\n");
    tx_bench_stop();
}

// ----------------------------------------------------------------------------
// Meta Information for the module
// ----------------------------------------------------------------------------

module_init(txctl_init);
module_exit(txctl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jonas Röckl <joroec@gmx.net>");
MODULE_DESCRIPTION("TX benchmark control");
