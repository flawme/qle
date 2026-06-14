#pragma once

#include "adapters/adapter.h"
#include <fstream>
#include <vector>

namespace qle {
namespace adapters {
namespace csv {

class CsvAdapter : public IAdapter {
public:
    CsvAdapter();
    ~CsvAdapter() override;
    
    void Open(const std::string& source) override;
    bool HasNext() override;
    Row Next() override;
    void Close() override;

private:
    int fd_;
    char* mmap_data_;
    size_t mmap_size_;
    size_t offset_;
    
    std::vector<std::string> headers_;

    void ReadHeaders();
};

} // namespace csv
} // namespace adapters
} // namespace qle
