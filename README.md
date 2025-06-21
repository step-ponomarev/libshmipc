# libshmipc

**Shared Memory IPC Library**
**Status:** Work in Progress

---

## Overview

`libshmipc` is a library designed for high-performance inter-process communication (IPC) using shared memory. It provides a lock-free transport layer for message passing between producers and consumers, and builds upon this with a configurable channel abstraction for higher-level IPC patterns.

---

## Architecture

### Transport Layer

At the core of the library is a circular buffer implemented over user-provided shared memory. This buffer enables:

* Lock-free writes and reads
* Atomic commit semantics
* Efficient wraparound handling
* Minimal memory overhead

The transport layer serves as the low-level mechanism for raw entry management, allowing fine-grained control over data layout and synchronization. This layer is ideal when users need direct control or wish to build custom semantics.

### Channel Layer

The channel layer wraps the buffer with additional retry logic, timeout support, and configurable backoff policies. It simplifies usage by offering:

* Non-blocking writes
* Blocking and timed reads
* Automatic retry on transient buffer states (e.g. not ready, locked)
* Configurable round-trip limits and sleep durations

Channels are suited for general producer-consumer communication without needing to manage entry flags or read state transitions explicitly.

---

## Example

```c
#include <shmipc/ipc_channel.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

uint8_t mem[1024];
IpcChannelConfiguration config = {
  .max_round_trips = 128,
  .start_sleep_ns = 1000,
  .max_sleep_ns = 100000
};

IpcChannel *chan = ipc_channel_create(mem, sizeof(mem), config);

int val = 123;
ipc_channel_write(chan, &val, sizeof(val));

IpcEntry entry;
IpcTransaction tx = ipc_channel_read(chan, &entry);
if (tx.status == IPC_OK) {
  printf("Received: %d\n", *(int*)entry.payload);
  free(entry.payload);
}

ipc_channel_destroy(chan);
```
