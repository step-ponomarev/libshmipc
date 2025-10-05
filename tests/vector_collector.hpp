#pragma once

#include <vector>
#include <mutex>
#include <unordered_set>

template <typename T>
class VectorCollector {
private:
    std::vector<T> data;
    std::mutex data_mutex;
    
public:
    void collect(T val) {
        std::lock_guard<std::mutex> lock(data_mutex);
        data.push_back(val);
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(data_mutex));
        return data.size();
    }
    
    std::unordered_set<T> get_all_collected() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(data_mutex));
        return std::unordered_set<T>(data.begin(), data.end());
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(data_mutex);
        data.clear();
    }
};
