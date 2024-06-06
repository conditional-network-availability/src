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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>


#define TX_BENCH_DURATION_S 10
#define LOCAL_SERVER_PORT 5555
#define BUF_LEN 4096

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


int main (int argc, char **argv) {
  int s, rc, n;
  unsigned int len;
  struct sockaddr_in cliAddr, servAddr;
  char buffer[BUF_LEN];
  const int y = 1;

  // create socket
  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
     printf("%s\n", strerror(errno));
     exit(EXIT_FAILURE);
  }

  // bind socket
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(LOCAL_SERVER_PORT);
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
  rc = bind(s, (struct sockaddr*) &servAddr, sizeof(servAddr));
  if (rc < 0) {
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

  printf("UDP echo with: %u\n", LOCAL_SERVER_PORT);

  memset(buffer, 0, BUF_LEN);
  len = sizeof(cliAddr);

  while(true) {
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    double start = spec.tv_sec*1000 + spec.tv_nsec/1e6;

    ssize_t ret = sendto(s, buffer, 1448, 0,
        (struct sockaddr*) &addrDest, sizeof(addrDest));
    if(ret < 0) {
      printf("%s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    n = recvfrom(s, buffer, BUF_LEN, 0, (struct sockaddr *) &cliAddr, &len);
    if (n < 0) {
      printf("%s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    // consider passage of time
    clock_gettime(CLOCK_REALTIME, &spec);
    double now = spec.tv_sec*1000 + spec.tv_nsec/1e6;
    double passed = now- start; // ms that passed

    printf("Millis: %f\n", passed);

    sleep(1);
  }

  return EXIT_SUCCESS;
}
