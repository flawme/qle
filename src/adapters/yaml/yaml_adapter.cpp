#include "adapters/yaml/yaml_adapter.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "security/path_validator.h"
#include "utils/mmap_compat.h"
#include <sys/stat.h>
#include <fcntl.h>


namespace qle {
namespace adapters {
namespace yaml {

YamlAdapter::YamlAdapter() : fd_(-1), mmap_data_(nullptr), mmap_size_(0), pos_(0), end_offset_(0), is_child_(false), has_next_(false), eof_reached_(false) {}

YamlAdapter::~YamlAdapter() {
    Close();
}

void YamlAdapter::Open(const std::string& source) {
    security::ValidateSourcePath(source);
    
    fd_ = open(source.c_str(), O_RDONLY);
    if (fd_ < 0) {
        throw errors::RuntimeError("Could not open YAML source: " + source);
    }

    struct stat sb;
    if (fstat(fd_, &sb) == -1) {
        throw errors::RuntimeError("Could not stat YAML source: " + source);
    }
    mmap_size_ = sb.st_size;
    
    if (mmap_size_ > security::Limits::Get().max_file_size) {
        throw errors::SecurityError("YAML file exceeds maximum allowed size.");
    }

    if (mmap_size_ > 0) {
        mmap_data_ = (char*)mmap(NULL, mmap_size_, PROT_READ, MAP_PRIVATE, fd_, 0);
        if (mmap_data_ == MAP_FAILED) {
            throw errors::RuntimeError("Failed to memory map YAML source");
        }
    }
    
    end_offset_ = mmap_size_;
    FetchNextRow();
}

bool YamlAdapter::HasNext() {
    return has_next_;
}

Row YamlAdapter::Next() {
    if (!has_next_) {
        throw errors::RuntimeError("No more rows to read.");
    }
    Row row = next_row_;
    FetchNextRow();
    return row;
}

void YamlAdapter::Close() {
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

std::vector<std::unique_ptr<IAdapter>> YamlAdapter::Split(size_t num_splits) {
    std::vector<std::unique_ptr<IAdapter>> splits;
    if (mmap_size_ == 0 || num_splits <= 1) return splits;
    
    size_t chunk_size = mmap_size_ / num_splits;
    for (size_t i = 0; i < num_splits; ++i) {
        auto child = std::make_unique<YamlAdapter>();
        child->fd_ = this->fd_;
        child->mmap_data_ = this->mmap_data_;
        child->mmap_size_ = this->mmap_size_;
        child->is_child_ = true;
        
        size_t start = i * chunk_size;
        size_t end = (i == num_splits - 1) ? mmap_size_ : (i + 1) * chunk_size;
        child->SetChunk(start, end);
        splits.push_back(std::move(child));
    }
    return splits;
}

void YamlAdapter::SetChunk(size_t start, size_t end) {
    end_offset_ = end;
    pos_ = start;
    if (pos_ == 0) {
        FetchNextRow();
    } else {
        while (pos_ < mmap_size_) {
            if (mmap_data_[pos_] == '\n') {
                size_t next = pos_ + 1;
                while (next < mmap_size_ && (mmap_data_[next] == ' ' || mmap_data_[next] == '\t')) next++;
                if (next < mmap_size_ && mmap_data_[next] == '-') {
                    if (next + 1 == mmap_size_ || mmap_data_[next+1] == ' ' || mmap_data_[next+1] == '\r' || mmap_data_[next+1] == '\n') {
                        pos_++; // Point to the start of the line (just after '\n')
                        break;
                    }
                }
            }
            pos_++;
        }
        FetchNextRow();
    }
}

bool YamlAdapter::GetLine(std::string& line) {
    if (pos_ >= mmap_size_) return false;
    size_t start = pos_;
    while (pos_ < mmap_size_ && mmap_data_[pos_] != '\n') {
        pos_++;
    }
    line = std::string(mmap_data_ + start, pos_ - start);
    if (pos_ < mmap_size_ && mmap_data_[pos_] == '\n') pos_++;
    return true;
}

void YamlAdapter::FetchNextRow() {
    has_next_ = false;
    if (eof_reached_ && lookahead_line_.empty()) {
        return;
    }

    Row row;
    bool row_started = false;

    if (!lookahead_line_.empty()) {
        ProcessLine(lookahead_line_, row);
        lookahead_line_.clear();
        row_started = true;
    } else if (pos_ >= end_offset_) {
        eof_reached_ = true;
        return;
    }

    std::string line;
    size_t line_start_pos = pos_;
    while (true) {
        line_start_pos = pos_;
        if (!GetLine(line)) break;
        
        std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue; 
        }

        if (trimmed.rfind("- ", 0) == 0 || trimmed == "-") {
            if (row_started) {
                if (line_start_pos >= end_offset_) {
                    // Start of next row is past our chunk boundary. Stop here.
                    eof_reached_ = true;
                    lookahead_line_.clear();
                } else {
                    lookahead_line_ = trimmed;
                }
                has_next_ = true;
                next_row_ = row;
                return;
            } else {
                row_started = true;
                ProcessLine(trimmed, row);
            }
        } else {
            if (row_started) {
                ProcessLine(trimmed, row);
            }
        }
    }

    eof_reached_ = true;
    if (row_started) {
        has_next_ = true;
        next_row_ = row;
    }
}

void YamlAdapter::ProcessLine(const std::string& line, Row& row) {
    std::string content = line;
    if (content.rfind("- ", 0) == 0) {
        content = content.substr(2);
    } else if (content == "-") {
        return; 
    }

    size_t colon_pos = content.find(':');
    if (colon_pos != std::string::npos) {
        std::string key = Trim(content.substr(0, colon_pos));
        std::string value = Trim(content.substr(colon_pos + 1));
        
        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || 
                                  (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }
        
        row[key] = value;
    }
}

std::string YamlAdapter::Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

} // namespace yaml
} // namespace adapters
} // namespace qle
