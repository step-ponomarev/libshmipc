#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

typedef void (*TestFunc)();

void run_test(const char *, TestFunc);

#endif
