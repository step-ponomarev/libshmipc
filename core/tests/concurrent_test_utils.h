#pragma once

#include "concurrency_manager.hpp"
#include "include/shmipc/ipc_common.h"
#include "shmipc/ipc_buffer.h"
#include "shmipc/ipc_channel.h"
#include "shmipc/ipc_common.h"
#include "test_utils.h"
#include "unsafe_collector.hpp"
#include <cstddef>

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

inline void consume_channel_with_timeout(IpcChannel *channel,
                                         UnsafeCollector<size_t> &collector,
                                         ConcurrencyManager<size_t> &manager,
                                         const timespec *timeout) {

  bool finished = false;
  while (true) {
    IpcEntry entry;

    finished = manager.all_producers_finished();
    IpcChannelReadResult result = ipc_channel_read(channel, &entry, timeout);
    if (result.ipc_status == IPC_OK) {
      size_t res;
      memcpy(&res, entry.payload, entry.size);
      collector.collect(res);
      free(entry.payload);
    } else if (finished && (result.ipc_status == IPC_EMPTY ||
                            result.ipc_status == IPC_ERR_TIMEOUT)) {
      break;
    }
  }
}
} // namespace concurrent_test_utils
