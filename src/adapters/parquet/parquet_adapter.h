#pragma once

#include "adapters/adapter.h"
#include <string>
#include <vector>
#include <memory>

// Forward declare the reader so we don't have to include all of tinyparquet in the header.
namespace tinyparquet {
    class Reader;
}

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
    std::unique_ptr<tinyparquet::Reader> reader_;
    std::vector<std::string> projected_cols_;
    
    // We store fully buffered string values for each projected column.
    // In a future optimized version, this could be chunked.
    std::vector<std::string> col_names_;
    std::vector<std::vector<std::string>> col_data_;
    
    size_t current_row_ = 0;
    size_t num_rows_ = 0;
};

} // namespace parquet
} // namespace adapters
} // namespace qle
