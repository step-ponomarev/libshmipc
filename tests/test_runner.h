#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*TestFunc)();

void run_test(const char *, TestFunc);

#ifdef __cplusplus
}
#endif

#endif
