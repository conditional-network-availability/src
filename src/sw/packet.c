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

#include "packet.h"

#include <string.h>
#include <common/debug.h>
#include <endian.h>
#include <arch_helpers.h>

/* -----------------------------------------------------------------------------
* Public Interface Declaration
* --------------------------------------------------------------------------- */

bool is_ipv4(struct ip_hdr_total* hdr) {
  uint16_t ether_prot = be16toh(hdr->eth.h_proto);
  // An ethernet protocol other than IP cannot be TCP/IP(v4)
  if(ether_prot != ETH_P_IP)
    return false;

  uint8_t ip_version = hdr->ip.ip_v;
  // We only support IPv4 at the moment
  if(ip_version != 0x4)
    return false;

  return true;
}
