# mumble-ping

This implements mumble's
[ping protocol](https://wiki.mumble.info/wiki/Protocol)
and outputs the gathered statistics as influxdb's 
[line protocol 2.0](https://docs.influxdata.com/influxdb/v2.0/reference/syntax/line-protocol/).

Goals for this project were:
  - trying out name resolution and UDP sockets in C
  - building a fully statically linked executable
  - compling with [musl libc](https://musl.libc.org) instead of glibc
  - having a portable (non-python) mumble scraper until [telegraf](https://github.com/influxdata/telegraf) supports mumble
