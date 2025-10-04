# Гайд по рефакторингу тестов libshmipc

## Обзор изменений

Был проведен полный рефакторинг тестовой базы проекта `libshmipc` с целью устранения дублирования кода, улучшения читаемости и упрощения поддержки тестов.

## Созданные утилиты

### 1. `test_utils.h` - Основные утилиты

#### RAII-обертки для автоматического управления памятью:

**`BufferWrapper`** - обертка для `IpcBuffer`:
```cpp
test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
// Автоматически создает буфер и освобождает память при выходе из области видимости
```

**`ChannelWrapper`** - обертка для `IpcChannel`:
```cpp
test_utils::ChannelWrapper channel(test_utils::SMALL_BUFFER_SIZE);
// Автоматически создает канал и освобождает память
```

**`EntryWrapper`** - обертка для `IpcEntry`:
```cpp
test_utils::EntryWrapper entry(sizeof(int));
// Автоматически выделяет и освобождает память для entry
```

#### Функции проверки статусов:

```cpp
test_utils::CHECK_OK(result);           // Проверяет успешность операции
test_utils::CHECK_ERROR(result, status); // Проверяет ошибку с конкретным статусом
```

#### Шаблонные функции для работы с данными:

```cpp
test_utils::write_data(buffer.get(), value);     // Запись данных
auto result = test_utils::read_data<int>(buffer.get()); // Чтение данных
auto peeked = test_utils::peek_data<int>(buffer.get()); // Просмотр данных
```

#### Константы для размеров:

```cpp
test_utils::SMALL_BUFFER_SIZE  = 128
test_utils::MEDIUM_BUFFER_SIZE = 256  
test_utils::LARGE_BUFFER_SIZE  = 1024
test_utils::DEFAULT_COUNT      = 200000
test_utils::LARGE_COUNT        = 300000
```

### 2. `concurrent_test_utils.h` - Утилиты для конкурентных тестов

#### Функции производителей и потребителей:

```cpp
concurrent_test_utils::produce_buffer(buffer, from, to);
concurrent_test_utils::consume_buffer(buffer, expected_count, dest);
concurrent_test_utils::produce_channel(channel, from, to);
concurrent_test_utils::consume_channel(channel, expected_count, dest);
```

#### Готовые тестовые сценарии:

```cpp
// Один писатель, один читатель
concurrent_test_utils::run_single_writer_single_reader_test(
    producer_func, consumer_func, count, buffer_size);

// Несколько писателей, один читатель  
concurrent_test_utils::run_multiple_writer_single_reader_test(
    producer_func, consumer_func, total, buffer_size);

// Несколько писателей, несколько читателей
concurrent_test_utils::run_multiple_writer_multiple_reader_test(
    producer_func, consumer_func, total, buffer_size);
```

#### Тестирование race conditions:

```cpp
concurrent_test_utils::test_race_between_skip_and_read_buffer();
concurrent_test_utils::test_race_between_skip_and_read_channel();
```

## Примеры рефакторинга

### До рефакторинга:
```cpp
TEST_CASE("single entry") {
    uint8_t mem[128];
    const IpcBufferCreateResult bufferResult = ipc_buffer_create(mem, 128);
    IpcBuffer *buffer = bufferResult.result;

    const int eval = 12;
    CHECK(IpcBufferWriteResult_is_ok(
        ipc_buffer_write(buffer, &eval, sizeof(eval))));

    IpcEntry entry = {.payload = malloc(sizeof(eval)), .size = sizeof(eval)};
    const IpcBufferReadResult result = ipc_buffer_read(buffer, &entry);
    CHECK(IpcBufferReadResult_is_ok(result));
    CHECK(entry.size == sizeof(eval));

    int res;
    memcpy(&res, entry.payload, entry.size);
    CHECK(res == eval);

    free(entry.payload);
    free(buffer);
}
```

### После рефакторинга:
```cpp
TEST_CASE("single entry") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    const int eval = 12;
    test_utils::write_data(buffer.get(), eval);
    
    const int result = test_utils::read_data<int>(buffer.get());
    CHECK(result == eval);
}
```

## Преимущества рефакторинга

### 1. **Устранение дублирования**
- Общий код вынесен в переиспользуемые утилиты
- Константы для размеров и конфигураций
- Единообразные функции проверки статусов

### 2. **Автоматическое управление памятью**
- RAII-обертки автоматически освобождают память
- Нет утечек памяти
- Нет необходимости в ручном `malloc`/`free`

### 3. **Улучшенная читаемость**
- Тесты стали короче и понятнее
- Шаблонные функции работают с любыми типами
- Четкое разделение ответственности

### 4. **Типобезопасность**
- Шаблонные функции `write_data<T>`, `read_data<T>`, `peek_data<T>`
- Автоматическое определение размеров типов
- Компилятор проверяет совместимость типов

### 5. **Переиспользование**
- Утилиты можно использовать в новых тестах
- Готовые сценарии для конкурентных тестов
- Единообразный стиль написания тестов

## Результаты

✅ **Все тесты проходят успешно:**
- `ipc_buffer_basic_test`: 16 тестов, 110 проверок
- `ipc_buffer_concurrency_test`: 5 тестов, 807,004 проверок  
- `ipc_channel_basic_test`: 14 тестов, 50 проверок
- `ipc_channel_concurrent_test`: 4 теста, 806,003 проверок
- `ipc_mmap_test`: 2 теста, 7 проверок

## Использование в новых тестах

**Важно:** Для использования утилит нужно включить правильные заголовки:

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "test_utils.h"
#include "concurrent_test_utils.h"  // для конкурентных тестов
#include "concurrent_set.hpp"       // для concurrent_set
#include <thread>
#include <vector>
#include <memory>
```

### Создание простого теста:
```cpp
TEST_CASE("my test") {
    test_utils::BufferWrapper buffer(test_utils::SMALL_BUFFER_SIZE);
    
    int value = 42;
    test_utils::write_data(buffer.get(), value);
    
    int result = test_utils::read_data<int>(buffer.get());
    CHECK(result == value);
}
```

### Создание конкурентного теста:
```cpp
TEST_CASE("concurrent test") {
    const uint64_t size = ipc_buffer_align_size(test_utils::SMALL_BUFFER_SIZE);
    const size_t count = 1000;

    std::vector<uint8_t> mem(size);
    const IpcBufferCreateResult buffer_result =
        ipc_buffer_create(mem.data(), size);
    IpcBuffer *buf = buffer_result.result;

    auto dest = std::make_shared<concurrent_set<size_t>>();

    std::thread producer(concurrent_test_utils::produce_buffer, buf, 0, count);
    std::thread consumer(concurrent_test_utils::consume_buffer, buf, count, dest);

    producer.join();
    consumer.join();

    CHECK(dest->size() == count);
    for (size_t i = 0; i < count; i++) {
        CHECK(dest->contains(i));
    }

    free(buf);
}
```

## Заключение

Рефакторинг значительно улучшил качество тестовой базы:
- **Убраны повторы** - код стал более поддерживаемым
- **Улучшена читаемость** - тесты легче понимать и модифицировать  
- **Автоматическое управление памятью** - нет утечек памяти
- **Типобезопасность** - меньше ошибок на этапе компиляции
- **Переиспользование** - утилиты можно использовать в новых тестах

Логика всех тестов сохранена полностью - они работают точно так же, как и раньше, но код стал намного чище и проще в поддержке.
