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

#define LOCAL_SERVER_PORT 5555
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

  printf("UDP echo with: %u\n", LOCAL_SERVER_PORT);

  memset(buffer, 0, BUF_LEN);
  len = sizeof(cliAddr);

  while(true) {
    n = recvfrom(s, buffer, BUF_LEN, 0, (struct sockaddr *) &cliAddr, &len);
    if (n < 0) {
      printf("%s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }

    ssize_t ret = sendto(s, buffer, n, 0,
        (struct sockaddr*) &cliAddr, sizeof(cliAddr));
    if(ret < 0) {
      printf("%s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  return EXIT_SUCCESS;
}
