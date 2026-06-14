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

    bool SupportsParallel() const override { return true; }
    std::vector<std::unique_ptr<IAdapter>> Split(size_t num_splits) override;
    void SetChunk(size_t start, size_t end);

private:
    int fd_;
    char* mmap_data_;
    size_t mmap_size_;
    size_t pos_;
    size_t end_offset_;
    bool is_child_ = false;
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
