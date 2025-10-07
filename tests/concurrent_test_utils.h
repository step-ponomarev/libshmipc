#pragma once

#include "concurrency_manager.hpp"
#include "shmipc/ipc_channel.h"
#include "shmipc/ipc_common.h"
#include "test_utils.h"
#include "unsafe_collector.hpp"
#include <chrono>
#include <cstring>
#include <thread>

namespace concurrent_test_utils {

inline void produce_buffer(IpcBuffer *buffer, size_t from, size_t to) {
  for (size_t i = from; i < to;) {
    IpcBufferWriteResult status = ipc_buffer_write(buffer, &i, sizeof(size_t));
    if (status.ipc_status != IPC_OK) {
      continue;
    }
    i++;
  }
}

inline void delayed_produce_buffer(IpcBuffer *buffer, size_t from, size_t to) {
  for (size_t i = from; i < to;) {
    void *dest;
    IpcBufferReserveEntryResult result =
        ipc_buffer_reserve_entry(buffer, sizeof(i), &dest);
    if (result.ipc_status != IPC_OK) {
      continue;
    }

    std::this_thread::sleep_for(std::chrono::microseconds(10));
    memcpy(dest, &i, sizeof(i));
    ipc_buffer_commit_entry(buffer, result.result);
    i++;
  }
}

inline void produce_channel(IpcChannel *channel, size_t from, size_t to) {
  for (size_t i = from; i < to;) {
    IpcChannelWriteResult status =
        ipc_channel_write(channel, &i, sizeof(size_t));
    if (status.ipc_status != IPC_OK) {
      continue;
    }
    i++;
  }
}

inline void consume_buffer(IpcBuffer *buffer,
                           UnsafeCollector<size_t> &collector,
                           ConcurrencyManager<size_t> &manager) {
  test_utils::EntryWrapper entry(sizeof(size_t));
  IpcEntry entry_ref = entry.get();

  bool finished = false;
  while (true) {
    finished = manager.all_producers_finished();
    IpcBufferReadResult result = ipc_buffer_read(buffer, &entry_ref);
    if (result.ipc_status == IPC_OK) {
      size_t res;
      memcpy(&res, entry_ref.payload, entry_ref.size);
      collector.collect(res);
    } else if (finished && result.ipc_status == IPC_EMPTY) {
      break;
    }
  }
}

inline void consume_channel(IpcChannel *channel,
                            UnsafeCollector<size_t> &collector,
                            ConcurrencyManager<size_t> &manager) {
  IpcEntry entry;
  bool finished = false;
  while (true) {
    finished = manager.all_producers_finished();
    IpcChannelTryReadResult result = ipc_channel_try_read(channel, &entry);
    if (result.ipc_status == IPC_OK) {
      size_t res;
      memcpy(&res, entry.payload, entry.size);
      collector.collect(res);
      free(entry.payload);
    } else if (finished && result.ipc_status == IPC_EMPTY) {
      break;
    }
  }
}

} // namespace concurrent_test_utils
