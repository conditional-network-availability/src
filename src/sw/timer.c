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

#include "timer.h"
#include "app/app.h"

#include <arch_helpers.h>

/* -----------------------------------------------------------------------------
* Global State
* --------------------------------------------------------------------------- */

static uint64_t current_freq;

/* -----------------------------------------------------------------------------
* Function Definitions
* --------------------------------------------------------------------------- */

void my_timer_init(void) {
  // Setting the timer
	uint64_t cval;
	uint32_t ctl = 0;

	// current_freq = 1; // every second
	current_freq = 100;
	cval = read_cntpct_el0() + read_cntfrq_el0() / current_freq;
	write_cntps_cval_el1(cval);

	/* Enable the secure physical timer */
	isb();
	set_cntp_ctl_enable(ctl);
	isb();
	write_cntps_ctl_el1(ctl);
	isb();

	// Finished setting the timer
}

void my_timer_handler(void) {
  /*
	 * Disable the timer. If we won't it will fire again immediately. The
	 * barriers ensure that there is no reordering of instructions around the
	 * reprogramming code.
	 */
	isb();
	write_cntps_ctl_el1(0);
	isb();

  // INFO("Timer handler\n");

  current_freq = pta_tick(current_freq);

	/* Reprogramm the timer */
	uint64_t cval;
	uint32_t ctl = 0;
	cval = read_cntpct_el0() + read_cntfrq_el0() / current_freq;
	write_cntps_cval_el1(cval);

	// Enable the secure physical timer
	isb();
	set_cntp_ctl_enable(ctl);
	isb();
	write_cntps_ctl_el1(ctl);
	isb();

}
