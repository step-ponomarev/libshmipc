#ifndef IPC_LOCK_ERNO_H
#define IPC_LOCK_ERNO_H

typedef enum {
  LOCK_OK = 0,
  LOCK_ERNO_INVALID_ARGUMENT = -1,
  LOCK_ERNO_NOT_HELD = -2,
  LOCK_ERNO_ERR = -3
} LockErno;

extern _Thread_local LockErno lock_erno;

#endif
