#pragma once

#include <unordered_set>
#include <vector>
#include <mutex>
#include <atomic>

template <typename T>
class SimpleCollector {
private:
    thread_local static std::unordered_set<T> local_data;
    static std::vector<std::unordered_set<T>> all_thread_data;
    static std::mutex finalize_mutex;
    
public:
    void collect(T val) {
        local_data.insert(val);
    }
    
    size_t local_size() const {
        return local_data.size();
    }
    
    static void finalize() {
        std::lock_guard<std::mutex> lock(finalize_mutex);
        all_thread_data.push_back(local_data);
    }
    
    static std::unordered_set<T> get_all_collected() {
        std::lock_guard<std::mutex> lock(finalize_mutex);
        std::unordered_set<T> result;
        for (const auto& thread_data : all_thread_data) {
            result.insert(thread_data.begin(), thread_data.end());
        }
        return result;
    }
    
    static void reset() {
        std::lock_guard<std::mutex> lock(finalize_mutex);
        all_thread_data.clear();
    }
};

// Static definitions
template <typename T>
thread_local std::unordered_set<T> SimpleCollector<T>::local_data;

template <typename T>
std::vector<std::unordered_set<T>> SimpleCollector<T>::all_thread_data;

template <typename T>
std::mutex SimpleCollector<T>::finalize_mutex;
