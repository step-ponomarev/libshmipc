# libshmipc

**WIP: Shared Memory IPC Library**  
Текущая стадия: базовый транспортный слой

---

## Что реализовано

На текущем этапе библиотека предоставляет:

- Циклический буфер поверх `shared memory`
- Безлоковую запись/чтение с использованием атомарных операций
- Простое API:
  - `ipc_write`, `ipc_read`, `ipc_peek`, `ipc_delete`
  - `ipc_reserve_entry` / `ipc_commit_entry` — для эффективной записи

Буфер реализован как **lock-free очередь с wraparound логикой** — эффективно использует выделенную память без доп. аллокаций.

---

## Примеры использования

```c
uint8_t mem[1024];
IpcBuffer *buf = ipc_buffer_create(mem, sizeof(mem));

int value = 42;
ipc_write(buf, &value, sizeof(value));

IpcEntry entry = {
  .payload = malloc(sizeof(int)),
  .size = sizeof(int),
};

if (ipc_read(buf, &entry) == IPC_OK) {
  printf("Read value: %d\n", *(int*)entry.payload);
}

free(entry.payload);
free(buf);

