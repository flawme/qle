#pragma once

#include "adapters/adapter.h"
#include <string>
#include <vector>

namespace qle {
namespace adapters {
namespace json {

class JsonAdapter : public IAdapter {
public:
    JsonAdapter();
    ~JsonAdapter() override;
    
    void Open(const std::string& source) override;
    bool HasNext() override;
    Row Next() override;
    void Close() override;

private:
    std::string content_;
    size_t pos_;
    bool is_open_;
    
    void SkipWhitespace();
    std::string ParseString();
    void Match(char expected);
    Row ParseObject();
    bool MoveToNextObject();
};

} // namespace json
} // namespace adapters
} // namespace qle
