#pragma once

#include <vector>
#include <unordered_set>
#include <cstddef>

template <typename T>
class UnsafeCollector {
private:
    std::vector<T> data;
    
public:
    void collect(T val) {
        data.push_back(val);
    }
    
    size_t size() const {
        return data.size();
    }
    
    std::unordered_set<T> get_all_collected() const {
        return std::unordered_set<T>(data.begin(), data.end());
    }
    
    void clear() {
        data.clear();
    }
};
