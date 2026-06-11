#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace qle {
namespace adapters {

using Row = std::map<std::string, std::string>;

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
