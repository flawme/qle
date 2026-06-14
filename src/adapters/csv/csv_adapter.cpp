#include "adapters/csv/csv_adapter.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "security/path_validator.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

namespace qle {
namespace adapters {
namespace csv {

CsvAdapter::CsvAdapter() : fd_(-1), mmap_data_(nullptr), mmap_size_(0), offset_(0), end_offset_(0), is_child_(false) {}

CsvAdapter::~CsvAdapter() {
    Close();
}

void CsvAdapter::Open(const std::string& source) {
    security::ValidateSourcePath(source);
    
    fd_ = open(source.c_str(), O_RDONLY);
    if (fd_ < 0) {
        throw errors::RuntimeError("Could not open CSV source: " + source);
    }

    struct stat sb;
    if (fstat(fd_, &sb) == -1) {
        throw errors::RuntimeError("Could not stat CSV source: " + source);
    }
    mmap_size_ = sb.st_size;
    
    if (mmap_size_ > security::Limits::Get().max_file_size) {
        throw errors::SecurityError("CSV file exceeds maximum allowed size.");
    }
    
    if (mmap_size_ > 0) {
        mmap_data_ = (char*)mmap(NULL, mmap_size_, PROT_READ, MAP_PRIVATE, fd_, 0);
        if (mmap_data_ == MAP_FAILED) {
            throw errors::RuntimeError("Failed to memory map CSV source");
        }
    }

    end_offset_ = mmap_size_;
    ReadHeaders();
}

void CsvAdapter::ReadHeaders() {
    if (offset_ >= mmap_size_) return;
    size_t end = offset_;
    while (end < mmap_size_ && mmap_data_[end] != '\n') end++;
    
    std::string line(mmap_data_ + offset_, end - offset_);
    offset_ = end < mmap_size_ ? end + 1 : mmap_size_;
    
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

void CsvAdapter::SetProjectedColumns(const std::vector<std::string>& cols) {
    projected_cols_ = cols;
}

bool CsvAdapter::HasNext() {
    return offset_ < end_offset_;
}

Row CsvAdapter::Next() {
    if (!HasNext()) {
        throw errors::RuntimeError("No more rows to read.");
    }

    Row row;
    size_t n_headers = headers_.size();
    size_t col_idx = 0;
    size_t cell_start = offset_;
    bool in_quotes = false;
    
    for (size_t i = offset_; i < mmap_size_; ++i) {
        char c = mmap_data_[i];
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ',' && !in_quotes) {
            if (col_idx < n_headers && is_projected_[col_idx]) {
                const char* start_ptr = mmap_data_ + cell_start;
                size_t len = i - cell_start;
                if (len > 0 && start_ptr[len - 1] == '\r') len--;
                if (len >= 2 && start_ptr[0] == '"' && start_ptr[len - 1] == '"') {
                    start_ptr++;
                    len -= 2;
                }
                row[headers_[col_idx]] = std::string(start_ptr, len);
            }
            col_idx++;
            cell_start = i + 1;
        } else if (c == '\n' && !in_quotes) {
            if (col_idx < n_headers && is_projected_[col_idx]) {
                const char* start_ptr = mmap_data_ + cell_start;
                size_t len = i - cell_start;
                if (len > 0 && start_ptr[len - 1] == '\r') len--;
                if (len >= 2 && start_ptr[0] == '"' && start_ptr[len - 1] == '"') {
                    start_ptr++;
                    len -= 2;
                }
                row[headers_[col_idx]] = std::string(start_ptr, len);
            }
            offset_ = i + 1;
            return row;
        }
    }
    
    // End of file
    if (cell_start < mmap_size_ && col_idx < n_headers && is_projected_[col_idx]) {
        const char* start_ptr = mmap_data_ + cell_start;
        size_t len = mmap_size_ - cell_start;
        if (len > 0 && start_ptr[len - 1] == '\r') len--;
        if (len >= 2 && start_ptr[0] == '"' && start_ptr[len - 1] == '"') {
            start_ptr++;
            len -= 2;
        }
        row[headers_[col_idx]] = std::string(start_ptr, len);
    }
    offset_ = mmap_size_;
    return row;
}

void CsvAdapter::Close() {
    if (!is_child_) {
        if (mmap_data_ && mmap_data_ != MAP_FAILED) {
            munmap(mmap_data_, mmap_size_);
            mmap_data_ = nullptr;
        }
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }
}

std::vector<std::unique_ptr<IAdapter>> CsvAdapter::Split(size_t num_splits) {
    std::vector<std::unique_ptr<IAdapter>> splits;
    if (mmap_size_ == 0 || num_splits <= 1) {
        return splits; // Not splittable or just 1 split requested
    }
    
    // We start parsing data from offset_ (after headers)
    size_t data_start = offset_;
    size_t data_size = mmap_size_ - data_start;
    size_t chunk_size = data_size / num_splits;
    
    for (size_t i = 0; i < num_splits; ++i) {
        auto child = std::make_unique<CsvAdapter>();
        child->fd_ = this->fd_;
        child->mmap_data_ = this->mmap_data_;
        child->mmap_size_ = this->mmap_size_;
        child->is_child_ = true;
        child->headers_ = this->headers_;
        child->projected_cols_ = this->projected_cols_;
        child->is_projected_ = this->is_projected_;
        
        size_t start = data_start + i * chunk_size;
        size_t end = (i == num_splits - 1) ? mmap_size_ : data_start + (i + 1) * chunk_size;
        child->SetChunk(start, end);
        splits.push_back(std::move(child));
    }
    return splits;
}

void CsvAdapter::SetChunk(size_t start, size_t end) {
    end_offset_ = end;
    
    // If we're not starting right at data_start, we must advance to the next newline
    // so we don't start in the middle of a line.
    size_t cur = start;
    if (cur > 0 && mmap_data_[cur - 1] != '\n') {
        while (cur < mmap_size_ && mmap_data_[cur] != '\n') cur++;
        if (cur < mmap_size_) cur++;
    }
    offset_ = cur;
}

} // namespace csv
} // namespace adapters
} // namespace qle
