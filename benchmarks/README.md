## Benchmarks

Этот каталог содержит Java бенчмарки ping-pong для сравнения IPC:
- Shared Memory (SHM)
- TCP loopback
- Unix domain socket

Запуск через `benchmarks/scripts/run_benchmarks.sh`.

### Быстрый старт

```bash
MESSAGE_COUNT=1000000 WARMUP_COUNT=100000 MESSAGE_SIZE=64 \
  ./benchmarks/scripts/run_benchmarks.sh all
```

### Параметры

- `MESSAGE_COUNT` — количество измеряемых round-trip.
- `WARMUP_COUNT` — количество прогревочных round-trip.
- `MESSAGE_SIZE` — размер сообщения в байтах.
  - Можно задавать одно число: `64`
  - Или список случайных размеров: `64,256,1024,4096,56000`

Минимальный размер сообщения автоматически приводится к 12 байтам.

### Рекомендуемые сценарии

Ниже три базовых сценария, которые стоит держать для сравнения.
Все запускаются с `MESSAGE_COUNT=1000000` и `WARMUP_COUNT=100000`.

#### 1) Небольшие данные

```bash
MESSAGE_COUNT=1000000 WARMUP_COUNT=100000 MESSAGE_SIZE=64 \
  ./benchmarks/scripts/run_benchmarks.sh all
```

#### 2) Большие данные

```bash
MESSAGE_COUNT=1000000 WARMUP_COUNT=100000 MESSAGE_SIZE=56000 \
  ./benchmarks/scripts/run_benchmarks.sh all
```

#### 3) Разные размеры

```bash
MESSAGE_COUNT=1000000 WARMUP_COUNT=100000 MESSAGE_SIZE="64,256,1024,4096,56000" \
  ./benchmarks/scripts/run_benchmarks.sh all
```

### Запуск отдельных сценариев

```bash
./benchmarks/scripts/run_benchmarks.sh shm
./benchmarks/scripts/run_benchmarks.sh tcp
./benchmarks/scripts/run_benchmarks.sh unix
```
