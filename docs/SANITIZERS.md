# Sanitizers для libshmipc

Проект поддерживает несколько sanitizers для поиска багов во время выполнения.

## Доступные Sanitizers

### 1. ThreadSanitizer (TSan) - Поиск Data Races

**Рекомендуется для этого проекта!** Находит гонки данных (data races) в многопоточном коде.

```bash
./scripts/test_with_tsan.sh
```

Или вручную:
```bash
mkdir build_tsan && cd build_tsan
cmake .. -DSHMIPC_ENABLE_TSAN=ON -DCMAKE_BUILD_TYPE=Debug
make
ctest
```

**Важно:** TSan нельзя использовать одновременно с ASan/MSan.

### 2. AddressSanitizer (ASan) - Поиск Memory Errors

Находит:
- Use-after-free
- Heap buffer overflow
- Stack buffer overflow
- Memory leaks
- Use-after-return

```bash
./scripts/test_with_asan.sh
```

Или вручную:
```bash
mkdir build_asan && cd build_asan
cmake .. -DSHMIPC_ENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug
make
ctest
```

### 3. UndefinedBehaviorSanitizer (UBSan) - Поиск Undefined Behavior

Находит:
- Integer overflow
- Division by zero
- Null pointer dereference
- Misaligned pointers
- Signed integer overflow

```bash
./scripts/test_with_ubsan.sh
```

Или вручную:
```bash
mkdir build_ubsan && cd build_ubsan
cmake .. -DSHMIPC_ENABLE_UBSAN=ON -DCMAKE_BUILD_TYPE=Debug
make
ctest
```

**Можно комбинировать с TSan или ASan:**
```bash
cmake .. -DSHMIPC_ENABLE_TSAN=ON -DSHMIPC_ENABLE_UBSAN=ON
```

## Интерпретация результатов

### ThreadSanitizer
Если TSan находит data race, вы увидите что-то вроде:
```
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 8 at 0x7b0400000000 by thread T2:
    #0 ipc_buffer_write src/ipc_buffer.c:123
    
  Previous read of size 8 at 0x7b0400000000 by thread T1:
    #0 ipc_buffer_read src/ipc_buffer.c:456
```

### AddressSanitizer
При обнаружении проблем с памятью:
```
ERROR: AddressSanitizer: heap-use-after-free on address 0x60300000eff0
READ of size 4 at 0x60300000eff0 thread T0
    #0 0x4ee66b in ipc_buffer_read src/ipc_buffer.c:234
```

### UndefinedBehaviorSanitizer
При обнаружении UB:
```
src/ipc_buffer.c:123:45: runtime error: signed integer overflow: 
2147483647 + 1 cannot be represented in type 'int'
```

## Производительность

Sanitizers замедляют выполнение:
- **TSan:** ~5-15x медленнее
- **ASan:** ~2x медленнее  
- **UBSan:** ~20% медленнее

Поэтому они используются только для отладки, не в production сборках.

## CI/CD Integration

Рекомендуется запускать sanitizers в CI:

```yaml
# Пример для GitHub Actions
- name: Test with ThreadSanitizer
  run: ./scripts/test_with_tsan.sh

- name: Test with AddressSanitizer
  run: ./scripts/test_with_asan.sh

- name: Test with UBSan
  run: ./scripts/test_with_ubsan.sh
```

## Отключение Sanitizers

По умолчанию все sanitizers выключены. Обычная сборка:
```bash
mkdir build && cd build
cmake ..
make
```

