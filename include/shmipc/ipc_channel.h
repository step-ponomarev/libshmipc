#ifndef IPC_CHANNEL_H
#define IPC_CHANNEL_H

#include "ipc_common.h"
#include <stdint.h>
#include <time.h>

// Идея
// Уровень канала умнее буфера. Умеет читать с таймаутом, освобождая CPU
// будет ли иметь буффер - вопрос. По-идее может содержать очередь - если канал
// забит будем писать туда. Но тут ли это нужно или выше. пока не знаю
//
typedef struct IpcChannel IpcChannel;

IpcChannel *ipc_channel_create(void *, const uint64_t);
IpcChannel *ipc_channel_connect(void *);
IpcStatus ipc_channel_destroy(IpcChannel *);

// запись никогда не блокирует
IpcStatus ipc_channel_write(IpcChannel *, const void *, const uint64_t);

// блокируещее чтение
IpcTransaction ipc_channel_read(IpcChannel *, IpcEntry *);
IpcStatus ipc_channel_read_with_timeout(IpcChannel *, IpcEntry *, time_t);

#endif
