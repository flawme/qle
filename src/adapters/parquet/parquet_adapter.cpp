#include "adapters/parquet/parquet_adapter.h"
#include "adapters/parquet/tinyparquet.hpp"
#include "errors/errors.h"
#include "security/path_validator.h"

namespace qle {
namespace adapters {
namespace parquet {

ParquetAdapter::ParquetAdapter() {}

ParquetAdapter::~ParquetAdapter() {
    Close();
}

void ParquetAdapter::SetProjectedColumns(const std::vector<std::string>& cols) {
    projected_cols_ = cols;
}

void ParquetAdapter::Open(const std::string& source) {
    security::ValidateSourcePath(source);
    
    try {
        reader_ = std::make_unique<tinyparquet::Reader>(source);
        auto metadata = reader_->GetMetaData();
        
        std::vector<std::string> to_read;
        if (projected_cols_.empty()) {
            for (size_t i = 1; i < metadata.schema.size(); ++i) {
                if (metadata.schema[i].num_children > 0) continue; // Skip nested
                to_read.push_back(metadata.schema[i].name);
            }
        } else {
            to_read = projected_cols_;
        }
        
        num_rows_ = metadata.num_rows;
        col_names_ = to_read;
        col_data_.resize(to_read.size());
        
        for (size_t i = 0; i < to_read.size(); ++i) {
            const std::string& col = to_read[i];
            
            // Find type in schema
            tinyparquet::Type type = tinyparquet::Type::BYTE_ARRAY; // Default string
            for (size_t j = 1; j < metadata.schema.size(); ++j) {
                if (metadata.schema[j].name == col) {
                    type = metadata.schema[j].type;
                    break;
                }
            }
            
            try {
                auto col_reader = reader_->GetColumnReader(col);
                if (type == tinyparquet::Type::INT32 || type == tinyparquet::Type::BOOLEAN) {
                    std::vector<int32_t> vals;
                    col_reader.ReadAllInt32(vals);
                    for (auto v : vals) col_data_[i].push_back(std::to_string(v));
                } else if (type == tinyparquet::Type::INT64) {
                    std::vector<int64_t> vals;
                    col_reader.ReadAllInt64(vals);
                    for (auto v : vals) col_data_[i].push_back(std::to_string(v));
                } else {
                    col_reader.ReadAllByteArray(col_data_[i]);
                }
                
                // Pad if short (malformed file etc)
                while (col_data_[i].size() < num_rows_) {
                    col_data_[i].push_back("");
                }
                
            } catch (const std::exception& e) {
                // If a column isn't found or fails (e.g. nested type request),
                // we gracefully pad it with empty strings to prevent failing the entire query.
                col_data_[i].assign(num_rows_, "");
            }
        }
        
        current_row_ = 0;
    } catch (const std::exception& e) {
        throw errors::RuntimeError("Failed to open Parquet file natively: " + std::string(e.what()));
    }
}

bool ParquetAdapter::HasNext() {
    return current_row_ < num_rows_;
}

Row ParquetAdapter::Next() {
    if (!HasNext()) {
        throw errors::RuntimeError("No more rows to read.");
    }
    Row row;
    for (size_t i = 0; i < col_names_.size(); ++i) {
        row.data.push_back({col_names_[i], col_data_[i][current_row_]});
    }
    current_row_++;
    return row;
}

void ParquetAdapter::Close() {
    reader_.reset();
    col_names_.clear();
    col_data_.clear();
    num_rows_ = 0;
    current_row_ = 0;
}

} // namespace parquet
} // namespace adapters
} // namespace qle
