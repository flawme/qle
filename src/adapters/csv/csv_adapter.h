#pragma once

#include "adapters/adapter.h"
#include <fstream>
#include <vector>

namespace qle {
namespace adapters {
namespace csv {

class CsvAdapter : public IAdapter {
public:
    CsvAdapter(char delimiter = ',');
    ~CsvAdapter() override;
    
    void Open(const std::string& source) override;
    bool HasNext() override;
    Row Next() override;
    void Close() override;
    void SetProjectedColumns(const std::vector<std::string>& cols) override;

    bool SupportsParallel() const override { return true; }
    std::vector<std::unique_ptr<IAdapter>> Split(size_t num_splits) override;
    void SetChunk(size_t start, size_t end);

private:
    int fd_;
    char* mmap_data_;
    size_t mmap_size_;
    size_t offset_;
    size_t end_offset_;
    bool is_child_ = false;
    char delimiter_;
    
    std::vector<std::string> headers_;
    std::vector<std::string> projected_cols_;
    std::vector<bool> is_projected_;

    void ReadHeaders();
};

} // namespace csv
} // namespace adapters
} // namespace qle
