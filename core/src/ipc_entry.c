#include <shmipc/ipc_entry.h>
#include <stdlib.h>

void ipc_entry_destroy(ipc_entry_t entry) {
    free(entry.payload);
}