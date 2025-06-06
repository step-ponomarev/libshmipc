# ipc

#Transport layer (ipc_buffer)

uint64_t ipc_allign_size(uint64_t) - выравнивает желаемый размер в большую сторону для эффективного использования буффера
IpcBuffer *ipc_buffer_attach(uint8_t *, const uint64_t); - подключаемся к участку буффера без его модификаций
IpcStatus ipc_buffer_init(IpcBuffer *); - сбрасываем буффер до начального состояния head==tail==0, необходимо вызвать хотя бы раз для рабочего состояния буффера, т.к. там задается размер буффера

IpcStatus ipc_write(IpcBuffer *, const void *, const uint64_t) пишем в конец очереди
IpcStatus ipc_read(IpcBuffer *, IpcEntry *); читаем запись из головы очереди
IpcStatus ipc_peek(IpcBuffer *, IpcEntry *); смотрим голову очереди
IpcStatus ipc_delete(IpcBuffer *); удаляем запись из головы, без копирований

