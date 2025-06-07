#ifndef IPC_UTILS_H
#define IPC_UTILS_H

#define ALIGN_UP(s, align) (((s) + ((align) - 1)) & ~((align) - 1))
#define RELATIVE(a, max) ((a) & (max - 1))

#endif
