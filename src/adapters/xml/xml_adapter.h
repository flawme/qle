#pragma once

#include "adapters/adapter.h"
#include <string>

namespace qle {
namespace adapters {
namespace xml {

class XmlAdapter : public IAdapter {
public:
    XmlAdapter();
    ~XmlAdapter() override;
    
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
    Row next_row_;
    std::string root_tag_;
    std::string row_tag_;

    void FetchNextRow();
    std::string ReadNextTag(bool& is_closing, std::string& text_before_tag);
};

} // namespace xml
} // namespace adapters
} // namespace qle
