// SPDX-License-Identifier: CC0-1.0

#define _POSIX_C_SOURCE 200809L
#include <unistd.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>

// https://wiki.mumble.info/wiki/Protocol

struct mumble_req {
  uint32_t type;
  uint64_t req_id;
} __attribute__((__packed__));

struct mumble_resp {
  uint8_t version[4];
  uint64_t resp_id;
  uint32_t users_curr;
  uint32_t users_max;
  uint32_t bandwidth;
} __attribute__((__packed__));


int main(int argc, char **argv) {

  char *host;
  char *port;

  switch (argc) {
    case 2:
      host = argv[1];
      port = "64738"; // default mumble port
      break;
    case 3:
      host = argv[1];
      port = argv[2];
      break;
    default:
      fprintf(stderr, "usage: %s HOST [PORT]\n", argv[0]);
      return 1;
  }

  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_CANONNAME;
  hints.ai_protocol = IPPROTO_UDP;

  struct addrinfo *result, *rp;

  int err;
  if ((err = getaddrinfo(host, port, &hints, &result))) {
    fprintf(stderr, "error resolving addr: %s\n", gai_strerror(err));
    return 1;
  }

  int sfd = -1;
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    if ((sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) != -1) {

      struct timeval timeout;
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      setsockopt(sfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
      setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

      if (rp == result && rp->ai_canonname != NULL) {
        host = strdup(rp->ai_canonname);

        // http://tools.ietf.org/html/draft-vixie-dnsext-dns0x20-00
        for (char *s = host; *s; ++s) *s = tolower(*s);

        // sanity check canonical name to prevent metric injection
        char c; int i = 0;
        while ((c = host[i++]) != '\0') {
          if (! (isalnum(c) || c == '.' || c == '-')) {
            fprintf(stderr, "got strange canonical name: %s\n", host);
            return 1;
          }
        }
      }

      break;
    }
  }

  if (sfd == -1) {
    fprintf(stderr, "could not open socket for %s:%s\n", host, port);
    return 1;
  }

  struct timespec rawtime;
  clock_gettime(0, &rawtime);

  struct mumble_req req = { 0, rawtime.tv_sec };
  ssize_t nsent = sendto(sfd, &req, sizeof(req), 0, rp->ai_addr, rp->ai_addrlen);
  switch (nsent) {
    case -1:
      perror("ping packet write");
      return 1;
    case sizeof(req):
      break; // we wrote the full packet
    default:
      fprintf(stderr, "partial outgoing ping packet\n");
      return 1;
  }

  freeaddrinfo(result);

  struct mumble_resp resp;
  ssize_t nread = recvfrom(sfd, &resp, sizeof(resp), MSG_WAITALL, NULL, NULL);
  switch (nread) {
    case -1:
      perror("ping packet read");
      return 1;
    case sizeof(resp):
      break; // we read the full packet
    default:
      fprintf(stderr, "partial incoming ping packet\n");
      return 1;
  }

  if (close(sfd) == -1) {
      perror("closing socket");
      return 1;
  }

  // https://docs.influxdata.com/influxdb/v2.0/reference/syntax/line-protocol/
  printf("mumble,host=%s,port=%s,version=%x.%x.%x users=%uu,max=%uu,bandwidth=%u %ld%09ld\n",
      host, port, resp.version[1], resp.version[2], resp.version[3],
      htonl(resp.users_curr), htonl(resp.users_max), htonl(resp.bandwidth),
      rawtime.tv_sec, rawtime.tv_nsec);

  return 0;
}
