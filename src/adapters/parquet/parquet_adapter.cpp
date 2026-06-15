#include "adapters/parquet/parquet_adapter.h"
#include "errors/errors.h"
#include "security/path_validator.h"

namespace qle {
namespace adapters {
namespace parquet {

ParquetAdapter::ParquetAdapter() : pipe_(nullptr), has_next_(false) {}

ParquetAdapter::~ParquetAdapter() {
    Close();
}

void ParquetAdapter::Open(const std::string& source) {
    security::ValidateSourcePath(source);
    
    // We use a Python script with pandas to convert parquet to CSV stream on the fly.
    // This maintains zero heavy C++ dependencies while supporting parquet natively.
    std::string cmd = "python3 -c \"import pandas as pd; import sys; "
                      "df = pd.read_parquet('" + source + "'); "
                      "df.to_csv(sys.stdout, index=False)\" 2>/dev/null";
                      
    pipe_ = popen(cmd.c_str(), "r");
    if (!pipe_) {
        throw errors::RuntimeError("Failed to open Parquet stream pipe.");
    }

    ReadHeaders();
    has_next_ = ReadLine();
    
    // If it's empty immediately, python might have failed
    if (!has_next_ && headers_.empty()) {
        int status = pclose(pipe_);
        pipe_ = nullptr;
        if (status != 0) {
            throw errors::RuntimeError("Python failed to read Parquet. Ensure pandas and pyarrow are installed (`pip install pandas pyarrow`).");
        }
    }
}

void ParquetAdapter::ReadHeaders() {
    if (!ReadLine()) return;
    
    std::string line = next_line_;
    size_t start = 0;
    size_t comma = 0;
    while ((comma = line.find(',', start)) != std::string::npos) {
        headers_.push_back(line.substr(start, comma - start));
        start = comma + 1;
    }
    headers_.push_back(line.substr(start));
    
    for (auto& h : headers_) {
        if (!h.empty() && h.back() == '\r') h.pop_back();
        if (h.size() >= 2 && h.front() == '"' && h.back() == '"') {
            h = h.substr(1, h.size() - 2);
        }
    }
    
    is_projected_.assign(headers_.size(), projected_cols_.empty());
    if (!projected_cols_.empty()) {
        for (size_t i = 0; i < headers_.size(); ++i) {
            for (const auto& p : projected_cols_) {
                if (headers_[i] == p) {
                    is_projected_[i] = true;
                    break;
                }
            }
        }
    }
}

void ParquetAdapter::SetProjectedColumns(const std::vector<std::string>& cols) {
    projected_cols_ = cols;
}

bool ParquetAdapter::ReadLine() {
    if (!pipe_) return false;
    char buffer[4096];
    next_line_.clear();
    while (fgets(buffer, sizeof(buffer), pipe_)) {
        next_line_ += buffer;
        if (!next_line_.empty() && next_line_.back() == '\n') {
            next_line_.pop_back(); // Remove newline
            return true;
        }
    }
    return !next_line_.empty();
}

bool ParquetAdapter::HasNext() {
    return has_next_;
}

Row ParquetAdapter::Next() {
    if (!has_next_) {
        throw errors::RuntimeError("No more rows to read.");
    }

    Row row;
    std::string line = next_line_;
    
    size_t n_headers = headers_.size();
    size_t col_idx = 0;
    size_t cell_start = 0;
    bool in_quotes = false;
    
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ',' && !in_quotes) {
            if (col_idx < n_headers && is_projected_[col_idx]) {
                std::string val = line.substr(cell_start, i - cell_start);
                if (!val.empty() && val.back() == '\r') val.pop_back();
                if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size()-2);
                row[headers_[col_idx]] = val;
            }
            col_idx++;
            cell_start = i + 1;
        }
    }
    
    if (col_idx < n_headers && is_projected_[col_idx]) {
        std::string val = line.substr(cell_start);
        if (!val.empty() && val.back() == '\r') val.pop_back();
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size()-2);
        row[headers_[col_idx]] = val;
    }

    has_next_ = ReadLine();
    
    if (!has_next_ && pipe_) {
        pclose(pipe_);
        pipe_ = nullptr;
    }

    return row;
}

void ParquetAdapter::Close() {
    if (pipe_) {
        pclose(pipe_);
        pipe_ = nullptr;
    }
}

} // namespace parquet
} // namespace adapters
} // namespace qle
