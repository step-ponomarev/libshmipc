#pragma once

#include <mutex>
#include <stddef.h>
#include <unordered_set>

template <typename T> class concurrent_set {
private:
  std::unordered_set<T> set;
  std::mutex mutex;

public:
  concurrent_set() = default;
  ~concurrent_set() = default;
  void insert(T val) {
    std::lock_guard<std::mutex> lock(mutex);
    set.insert(val);
  }

  bool contains(T val) {
    std::lock_guard<std::mutex> lock(mutex);
    return set.contains(val);
  }

  size_t size() {
    std::lock_guard<std::mutex> lock(mutex);
    return set.size();
  }
};
