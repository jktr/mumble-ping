# mumble-ping

This implements mumble's
[ping protocol](https://wiki.mumble.info/wiki/Protocol)
and outputs the gathered statistics in influxdb's
[line protocol 2.0](https://docs.influxdata.com/influxdb/v2.0/reference/syntax/line-protocol/)
or as JSON.

Goals for this project were:
  - trying out name resolution and UDP sockets in C
  - building a fully statically linked executable with nix
  - compling with [musl libc](https://musl.libc.org) instead of glibc
  - having a portable (non-python) mumble scraper until [telegraf](https://github.com/influxdata/telegraf) supports mumble

## example:

```
$ ./mumble-ping --influx mumble.example.com
mumble,server=mumble.example.com,port=64738,version=1.3.0 users=3u,users_max=100u,bandwidth=125000u,latency=15u 1610319359057131867
$ ./mumble-ping --json mumble.example.com
{"server":"mumble.example.com","port":64738,"version":"1.3.0","users":3,"users_max":100,"bandwidth":125000,"latency":15,"timestamp":1610319359057131867}
```
| tag/field | relation to server | unit |
| --- | --- | --- |
| server | canonical DNS name | |
| port | port, ignoring SRV records | |
| version | version number, ignoring zeroth component | |
| users | currently connected users | |
| users_max | maximum user capacity | |
| bandwith | per-user bandwidth cap | bits/second |
| latency | .5 RTT | milliseconds |

Note that the measurement's nanosecond timestamp is adjusted for server latency.
