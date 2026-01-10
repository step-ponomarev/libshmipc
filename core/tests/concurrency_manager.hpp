#pragma once

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

template <typename T> class ConcurrencyManager {
private:
  std::atomic<int> active_producers{0};
  std::atomic<bool> producers_finished{false};
  std::vector<std::function<void()>> producer_tasks;
  std::vector<std::function<void()>> consumer_tasks;

public:
  template <typename ProducerFunc, typename... Args>
  void add_producer(ProducerFunc &&func, Args &&...args) {
    active_producers.fetch_add(1);
    producer_tasks.emplace_back([this, func, args...]() {
      func(args...);
      producer_finished();
    });
  }

  template <typename ConsumerFunc, typename... Args>
  void add_consumer(ConsumerFunc &&func, Args &&...args) {
    consumer_tasks.emplace_back([func, args...]() { func(args...); });
  }

  ConcurrencyManager &get_manager() { return *this; }

  void run_and_wait() {
    std::vector<std::thread> producer_threads;
    std::vector<std::thread> consumer_threads;

    for (auto &task : producer_tasks) {
      producer_threads.emplace_back(task);
    }

    for (auto &task : consumer_tasks) {
      consumer_threads.emplace_back(task);
    }

    for (auto &thread : producer_threads) {
      if (thread.joinable()) {
        thread.join();
      }
    }

    for (auto &thread : consumer_threads) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

  bool all_producers_finished() const { return producers_finished.load(); }

private:
  void producer_finished() {
    int remaining = active_producers.fetch_sub(1) - 1;
    if (remaining == 0) {
      producers_finished.store(true);
    }
  }
};
