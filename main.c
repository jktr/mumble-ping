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

#define DEFAULT_MUMBLE_PORT "64738"

#define FORMAT_DEFAULT 0u
#define FORMAT_INFLUX 1u
#define FORMAT_JSON 2u

#define USAGE fprintf(stderr, "usage: %s [--json|--influx] HOST [PORT]\n", argv[0])

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


// https://ftp.gnu.org/old-gnu/Manuals/glibc-2.2.5/html_node/Elapsed-Time.html
int timespec_subtract (struct timespec *result, struct timespec *x, struct timespec *y) {
  if (x->tv_nsec < y->tv_nsec) {
    int nsec = (y->tv_nsec - x->tv_nsec) / 1000000000 + 1;
    y->tv_nsec -= 1000000000 * nsec;
    y->tv_sec += nsec;
  }

  if (x->tv_nsec - y->tv_nsec > 1000000000) {
    int nsec = (x->tv_nsec - y->tv_nsec) / 1000000000;
    y->tv_nsec += 1000000000 * nsec;
    y->tv_sec -= nsec;
  }

  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_nsec = x->tv_nsec - y->tv_nsec;

  return x->tv_sec < y->tv_sec;
}

int main(int argc, char **argv) {

  char *host;
  char *port;
  uint8_t format = FORMAT_DEFAULT;

  switch (argc) {
    case 2:
      host = argv[1];
      port = DEFAULT_MUMBLE_PORT;
      break;
    case 3:
      if (!strcmp(argv[1], "--json")) {
        format = FORMAT_JSON;
      } else if (!strcmp(argv[1], "--influx")) {
        format = FORMAT_INFLUX;
      }
      if (format != FORMAT_DEFAULT) {
        host = argv[2];
        port = DEFAULT_MUMBLE_PORT;
      } else {
        host = argv[1];
        port = argv[2];
      }
      break;
    case 4:
      host = argv[2];
      port = argv[3];
      if (!strcmp(argv[1], "--json")) {
        format = FORMAT_JSON;
        break;
      } else if (!strcmp(argv[1], "--influx")) {
        format = FORMAT_INFLUX;
        break;
      }
      __attribute__((fallthrough));
    default:
      USAGE;
      return 1;
  }

  if (host[0] == '\0' || host[0] == '-' \
      || port[0] == '\0' || port[0] == '-') {
    USAGE;
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

  struct timespec pretime, posttime, walltime;
  clock_gettime(CLOCK_REALTIME, &walltime);
  clock_gettime(CLOCK_MONOTONIC, &pretime);

  struct mumble_req req = { 0, pretime.tv_sec };
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

  clock_gettime(CLOCK_MONOTONIC, &posttime);

  if (close(sfd) == -1) {
      perror("closing socket");
      return 1;
  }

  struct timespec rtt;
  timespec_subtract(&rtt, &posttime, &pretime);
  long latency_ms = (rtt.tv_sec * 1000 + rtt.tv_nsec / 1000000) / 2;

  // estimate server time as .5 RTT
  walltime.tv_sec  += latency_ms / 1000;
  walltime.tv_nsec += (latency_ms % 1000) * 1000000;

  switch (format) {
    case FORMAT_JSON:
      printf("{\"server\":\"%s\",\"port\":%s,\"version\":\"%x.%x.%x\","
            "\"users\":%u,\"users_max\":%u,\"bandwidth\":%u,\"latency\":%ld,"
            "\"timestamp\":%ld%09ld}\n",
          host, port, resp.version[1], resp.version[2], resp.version[3],
          htonl(resp.users_curr), htonl(resp.users_max), htonl(resp.bandwidth),
          latency_ms, walltime.tv_sec, walltime.tv_nsec);

      break;
    case FORMAT_INFLUX:
    case FORMAT_DEFAULT:
      // https://docs.influxdata.com/influxdb/v2.0/reference/syntax/line-protocol/
      printf("mumble,server=%s,port=%s,version=%x.%x.%x"
            " users=%uu,users_max=%uu,bandwidth=%uu,latency=%ldu"
            " %ld%09ld\n",
          host, port, resp.version[1], resp.version[2], resp.version[3],
          htonl(resp.users_curr), htonl(resp.users_max), htonl(resp.bandwidth),
          latency_ms, walltime.tv_sec, walltime.tv_nsec);
  }

  return 0;
}
