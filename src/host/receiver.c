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
#define LOCAL_SERVER_PORT 6666
#define BUF_LEN 4096


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

  printf("Wating for data at port (UDP): %u\n", LOCAL_SERVER_PORT);

  memset(buffer, 0, BUF_LEN);
  len = sizeof(cliAddr);

	uint64_t consumed_bytes = 0;
  bool bench_running = false;
  uint64_t bench_started = 0;

  while (1) {
    n = recvfrom(s, buffer, BUF_LEN, 0, (struct sockaddr *) &cliAddr, &len);
    if (n < 0) {
      printf("%s\n", strerror(errno));
      continue;
    }

    if(bench_running) {

      // add bytes
      consumed_bytes += n;

      // consider passage of time
      struct timespec spec;
      clock_gettime(CLOCK_REALTIME, &spec);
      uint64_t millis = spec.tv_sec*1000 + lround(spec.tv_nsec/1e6);
      uint64_t passed = millis - bench_started; // ms that passed
      // printf("passed: %ld\n", passed);

      if(passed >= TX_BENCH_DURATION_S * 1000) {
        // benchmark is finished
        uint64_t mbit = ((consumed_bytes / 1000000) * 8);
        uint64_t mbits = mbit / TX_BENCH_DURATION_S;
        printf("TX benchark finished. Bytes: %lx, Time(s): %d, Mbit: %ld, CIRCA (calc yourself) Mbit/s: %ld\n",
          consumed_bytes, TX_BENCH_DURATION_S, mbit, mbits);

        // reset benchmark
        bench_running = false;
        consumed_bytes = 0;
      }

    } else {

      if(n > 0) {
        printf("TX benchmark started\n");
        bench_running = true;

        // start time
        struct timespec spec;
        clock_gettime(CLOCK_REALTIME, &spec);
        uint64_t millis = spec.tv_sec*1000 + lround(spec.tv_nsec/1e6);
        bench_started = millis;
      }

    }

  }

  return EXIT_SUCCESS;
}
