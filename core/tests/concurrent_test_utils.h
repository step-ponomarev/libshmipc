#pragma once

#include "concurrency_manager.hpp"
#include "shmipc/ipc_buffer.h"
#include "shmipc/ipc_channel.h"
#include "test_utils.h"
#include "unsafe_collector.hpp"

namespace concurrent_test_utils {

inline void produce_buffer(ipc_buffer_t *buffer, size_t from, size_t to) {
  for (size_t i = from; i < to;) {
    const ipc_status_t status =
        ipc_buffer_write(buffer, &i, sizeof(size_t), nullptr);
    if (status != IPC_STATUS_OK) {
      continue;
    }
    i++;
  }
}

inline void produce_channel(ipc_channel_t *channel, size_t from, size_t to) {
  for (size_t i = from; i < to;) {
    const ipc_status_t status =
        ipc_channel_write(channel, &i, sizeof(size_t), nullptr);
    if (status != IPC_STATUS_OK) {
      continue;
    }
    i++;
  }
}

inline void consume_buffer(ipc_buffer_t *buffer,
                           UnsafeCollector<size_t> &collector,
                           ConcurrencyManager<size_t> &manager) {
  test_utils::EntryWrapper entry(sizeof(size_t));
  ipc_entry_t entry_ref = entry.get();

  bool finished = false;
  while (true) {
    finished = manager.all_producers_finished();
    const ipc_status_t status = ipc_buffer_read(buffer, &entry_ref, nullptr);
    if (status == IPC_STATUS_OK) {
      size_t res;
      memcpy(&res, entry_ref.payload, entry_ref.size);
      collector.collect(res);
    } else if (finished && status == IPC_STATUS_EMPTY) {
      break;
    }
  }
}

inline void consume_channel(ipc_channel_t *channel,
                            UnsafeCollector<size_t> &collector,
                            ConcurrencyManager<size_t> &manager) {
  ipc_entry_t entry;
  bool finished = false;
  while (true) {
    finished = manager.all_producers_finished();
    const ipc_status_t status = ipc_channel_try_read(channel, &entry, nullptr);
    if (status == IPC_STATUS_OK) {
      size_t res;
      memcpy(&res, entry.payload, entry.size);
      collector.collect(res);
      free(entry.payload);
    } else if (finished && status == IPC_STATUS_EMPTY) {
      break;
    }
  }
}

inline void consume_channel_with_timeout(ipc_channel_t *channel,
                                         UnsafeCollector<size_t> &collector,
                                         ConcurrencyManager<size_t> &manager,
                                         const timespec *timeout) {

  bool finished = false;
  while (true) {
    ipc_entry_t entry;

    finished = manager.all_producers_finished();
    const ipc_status_t status = ipc_channel_read(channel, &entry, timeout, nullptr);
    if (status == IPC_STATUS_OK) {
      size_t res;
      memcpy(&res, entry.payload, entry.size);
      collector.collect(res);
      free(entry.payload);
    } else if (finished && (status == IPC_STATUS_EMPTY ||
                            status == IPC_STATUS_TIMEOUT)) {
      break;
    }
  }
}
} // namespace concurrent_test_utils
