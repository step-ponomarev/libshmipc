#include "test_runner.h"
#include <stdio.h>

void run_test(const char *name, const TestFunc fn) {
  fprintf(stdout, "[TEST] %s\n", name);
  fn();
  fprintf(stdout, "[PASSED] %s\n", name);
}
