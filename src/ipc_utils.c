#include "ipc_utils.h"
#include "shmipc/ipc_common.h"

inline IpcTransaction ipc_create_transaction(const uint64_t id,
                                             const IpcStatus status) {
  return (IpcTransaction){.entry_id = id, .status = status};
}
