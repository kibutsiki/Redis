# Redis Server Project Statistics

Last updated: 2026-10-24

## Project summary

- Implemented a C++ TCP key-value server inspired by Redis.
- Added a nonblocking socket event loop using `poll()`.
- Added length-prefixed request and response framing.
- Added command parsing for `GET`, `SET`, and `DEL`.
- Replaced `std::map` storage with an intrusive hash table using separate chaining.
- Added incremental hash-table structure support with newer and older tables.

## Current codebase

| Area | Details |
|---|---|
| Main server | `server.cpp` - 441 lines |
| Hash-table interface | `hashtable.h` - 27 lines |
| Hash-table implementation | `hashtable.cpp` - 125 lines |
| Hash-table buckets | `std::vector<HNode*>` |
| Collision handling | Intrusive linked-list separate chaining |
| Hash function | 64-bit FNV-1a |
| Network model | Nonblocking TCP sockets with `poll()` |
| Supported commands | `GET`, `SET`, `DEL` |
| Maximum message size | 32 MiB |
| Maximum parsed arguments | 200,000 |

## Validation

- Compiled with:

  ```bash
  g++ -std=c++17 -Wall -Wextra -pedantic -O0 -g \
      server.cpp hashtable.cpp -o /tmp/redis-server
  ```

- End-to-end TCP protocol test passed.
- Verified missing-key lookup returns `RES_NX`.
- Verified `SET` followed by `GET` returns the stored value.
- Verified updating an existing key replaces its value.
- Verified `DEL` removes a key.
- Verified lookup after deletion returns `RES_NX`.
- Test coverage in the validation run: 7 protocol operations.

## Performance baseline

Measured on localhost with the server compiled using `g++ -std=c++17 -O2`
and a Python TCP client. The workload performed 5,000 `SET` operations and
5,000 `GET` operations using 10,000 total requests.

| Metric | Result |
|---|---:|
| Sequential throughput | 3,650.20 operations/sec |
| Sequential average round-trip time | 273.96 us |
| Pipelined throughput | 91,997.89 operations/sec |
| Pipelined average response time | 10.87 us |
| Single-request median latency | 206.24 us |
| Single-request p95 latency | 394.50 us |
| Single-request minimum latency | 76.03 us |
| Single-request maximum latency | 6,403.73 us |

Benchmark details:

- Sequential run: 10,000 requests, one request followed by one response.
- Pipelined run: 10,000 requests sent before reading responses.
- Latency run: 1,000 individual `GET` requests for an existing key.
- Transport: loopback TCP (`127.0.0.1:1234`).
- These values are a baseline for future optimizations, not a comparison
  against the original `std::map` implementation.

## Resume-ready description

> Developed a C++ nonblocking TCP key-value server with `poll()`-based event handling, length-prefixed binary request/response framing, and `GET`/`SET`/`DEL` command processing. Implemented an intrusive separate-chaining hash table with 64-bit FNV-1a hashing and validated the server through end-to-end TCP integration tests.
