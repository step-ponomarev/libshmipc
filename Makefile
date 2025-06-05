CC = cc
CFLAGS = -std=c11 -Wall -Wextra -g
LIBS = $(shell pkg-config --libs --cflags criterion)

SRC = \
	src/ipc_mmap.c \
	src/ipc_buffer.c \
	src/lock/read_write_lock.c \
	src/lock/lock_erno.c

TESTS = tests/test.c

test: $(SRC) $(TESTS)
	$(CC) $(CFLAGS) -o test_runner $(SRC) $(TESTS) $(LIBS)
	./test_runner
	rm ./test_runner
