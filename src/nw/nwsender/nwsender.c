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

#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <memory.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>


const char* SERVER_IP = "192.168.188.29";
const char* SERVER_PORT = "5555";


int resolvehelper(const char* hostname, int family, const char* service,
    struct sockaddr_storage* pAddr) {
  int result;
  struct addrinfo* result_list = NULL;
  struct addrinfo hints = {};
  hints.ai_family = family;
  hints.ai_socktype = SOCK_DGRAM;
  result = getaddrinfo(hostname, service, &hints, &result_list);
  if (result == 0) {
    memcpy(pAddr, result_list->ai_addr, result_list->ai_addrlen);
    freeaddrinfo(result_list);
  }
  return result;
}


int main() {
    int rc = 0;

    // create socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
      printf("%s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    // bind socket
    struct sockaddr_in addrListen = {};
    addrListen.sin_family = AF_INET;
    rc = bind(sock, (struct sockaddr*)&addrListen, sizeof(addrListen));
    if(rc < 0) {
      printf("%s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    // resolve destination address
    struct sockaddr_storage addrDest = {};
    rc = resolvehelper(SERVER_IP, AF_INET, SERVER_PORT, &addrDest);
    if(rc != 0) {
      printf("%s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    char data[1448];
    memset(data, 0x42, 1448);

    uint64_t bytes_sent = 0;
    while(1) {
      ssize_t ret = sendto(sock, data, 1448, 0,
          (struct sockaddr*)&addrDest, sizeof(addrDest));
      if(ret < 0) {
        printf("%s\n", strerror(errno));
        exit(EXIT_FAILURE);
      }

      bytes_sent += ret;
    }

    return 0;
}
