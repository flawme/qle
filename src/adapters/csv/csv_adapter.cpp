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

CsvAdapter::CsvAdapter() : fd_(-1), mmap_data_(nullptr), mmap_size_(0), offset_(0) {}

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
}

bool CsvAdapter::HasNext() {
    return offset_ < mmap_size_;
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
            if (col_idx < n_headers) {
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
            if (col_idx < n_headers) {
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
    if (cell_start < mmap_size_ && col_idx < n_headers) {
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
    if (mmap_data_ && mmap_data_ != MAP_FAILED) {
        munmap(mmap_data_, mmap_size_);
        mmap_data_ = nullptr;
    }
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

} // namespace csv
} // namespace adapters
} // namespace qle
