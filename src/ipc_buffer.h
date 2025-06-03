#ifndef IPC_BUFF_H
#define IPC_BUFF_H

#include <stdint.h>

typedef struct IpcBuffer IpcBuffer;

typedef struct IpcEntry {
  uint64_t size;
  void *payload;
} IpcEntry;

typedef enum { IPC_OK = 0, IPC_ERR_INVALID_SIZE = -1 } IpcStatus;

IpcBuffer *ipc_buffer_attach(uint8_t *, const uint64_t);
IpcStatus ipc_buffer_init(IpcBuffer *);

char ipc_write(IpcBuffer *, const void *, const uint64_t);
IpcEntry ipc_read(IpcBuffer *);

#endif
