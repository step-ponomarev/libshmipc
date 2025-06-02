#ifndef IPC_BUFF_H
#define IPC_BUFF_H

#include <stddef.h>
typedef struct IpcBuffer IpcBuffer;

typedef struct IpcEntry { 
  void * payload;
  size_t size;
} IpcEntry;

IpcBuffer* ipc_create_buffer(const char*);
void ipc_write(IpcBuffer*, const void*, const size_t);
char ipc_has_message(const IpcBuffer*);
IpcEntry ipc_read(IpcBuffer*);

#endif
