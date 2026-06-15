#pragma once

#include "adapters/adapter.h"
#include <string>
#include <vector>
#include <cstdio>

namespace qle {
namespace adapters {
namespace parquet {

class ParquetAdapter : public IAdapter {
public:
    ParquetAdapter();
    ~ParquetAdapter() override;
    
    void Open(const std::string& source) override;
    bool HasNext() override;
    Row Next() override;
    void Close() override;
    void SetProjectedColumns(const std::vector<std::string>& cols) override;

private:
    FILE* pipe_;
    std::vector<std::string> headers_;
    std::vector<std::string> projected_cols_;
    std::vector<bool> is_projected_;
    bool has_next_;
    std::string next_line_;

    void ReadHeaders();
    bool ReadLine();
};

} // namespace parquet
} // namespace adapters
} // namespace qle
