#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include <memory>
#include <functional>

template <typename T>
class ConcurrencyManager {
private:
    std::atomic<int> active_producers{0};
    std::atomic<bool> producers_finished{false};
    std::vector<std::function<void()>> producer_tasks;
    std::vector<std::function<void()>> consumer_tasks;
    
public:
    // Добавить продюсера
    template<typename ProducerFunc, typename... Args>
    void add_producer(ProducerFunc&& func, Args&&... args) {
        active_producers.fetch_add(1);
        producer_tasks.emplace_back([this, func, args...]() {
            func(args...);
            producer_finished();
        });
    }
    
    // Добавить консьюмера
    template<typename ConsumerFunc, typename... Args>
    void add_consumer(ConsumerFunc&& func, Args&&... args) {
        consumer_tasks.emplace_back([func, args...]() {
            func(args...);
        });
    }
    
    // Получить ссылку на менеджер для передачи в консьюмеры
    ConcurrencyManager& get_manager() {
        return *this;
    }
    
    // Запустить все потоки и дождаться завершения
    void run_and_wait() {
        std::vector<std::thread> producer_threads;
        std::vector<std::thread> consumer_threads;
        
        // Создаем потоки для продюсеров
        for (auto& task : producer_tasks) {
            producer_threads.emplace_back(task);
        }
        
        // Создаем потоки для консьюмеров
        for (auto& task : consumer_tasks) {
            consumer_threads.emplace_back(task);
        }
        
        // Ждем завершения всех продюсеров
        for (auto& thread : producer_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        // Ждем завершения всех консьюмеров
        for (auto& thread : consumer_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
    
    // Проверить, завершили ли все продюсеры
    bool all_producers_finished() const {
        return producers_finished.load();
    }
private:
    void producer_finished() {
        int remaining = active_producers.fetch_sub(1) - 1;
        if (remaining == 0) {
            producers_finished.store(true);
        }
    }
};
