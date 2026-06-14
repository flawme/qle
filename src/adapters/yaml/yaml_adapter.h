#pragma once

#include "adapters/adapter.h"
#include <string>
#include <vector>

namespace qle {
namespace adapters {
namespace yaml {

class YamlAdapter : public IAdapter {
public:
    YamlAdapter();
    ~YamlAdapter() override;
    
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

    bool has_next_;
    bool eof_reached_;
    Row next_row_;
    std::string lookahead_line_;

    void FetchNextRow();
    void ProcessLine(const std::string& line, Row& row);
    std::string Trim(const std::string& s);
    bool GetLine(std::string& line);
};

} // namespace yaml
} // namespace adapters
} // namespace qle
