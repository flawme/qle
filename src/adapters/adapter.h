#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace qle {
namespace adapters {

class Row {
public:
    std::vector<std::pair<std::string, std::string>> data;

    std::string& operator[](const std::string& key) {
        for (auto& pair : data) {
            if (pair.first == key) return pair.second;
        }
        data.push_back({key, ""});
        return data.back().second;
    }
    
    auto find(const std::string& key) const {
        for (auto it = data.begin(); it != data.end(); ++it) {
            if (it->first == key) return it;
        }
        return data.end();
    }
    
    auto find(const std::string& key) {
        for (auto it = data.begin(); it != data.end(); ++it) {
            if (it->first == key) return it;
        }
        return data.end();
    }
    
    auto end() const { return data.end(); }
    auto begin() const { return data.begin(); }
    auto end() { return data.end(); }
    auto begin() { return data.begin(); }
    size_t size() const { return data.size(); }
    void clear() { data.clear(); }
};

class IAdapter {
public:
    virtual ~IAdapter() = default;
    
    virtual void Open(const std::string& source) = 0;
    virtual bool HasNext() = 0;
    virtual Row Next() = 0;
    virtual void Close() = 0;
};

} // namespace adapters
} // namespace qle
