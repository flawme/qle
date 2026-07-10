#include "adapters/json/json_adapter.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "security/path_validator.h"
#include "utils/mmap_compat.h"
#include <sys/stat.h>
#include <fcntl.h>

#include <cctype>

namespace qle {
namespace adapters {
namespace json {

JsonAdapter::JsonAdapter() : fd_(-1), mmap_data_(nullptr), mmap_size_(0), pos_(0), end_offset_(0), is_child_(false), is_open_(false) {}

JsonAdapter::~JsonAdapter() {
    Close();
}

void JsonAdapter::Open(const std::string& source) {
    security::ValidateSourcePath(source);
    
    fd_ = open(source.c_str(), O_RDONLY);
    if (fd_ < 0) {
        throw errors::RuntimeError("Could not open JSON source: " + source);
    }

    struct stat sb;
    if (fstat(fd_, &sb) == -1) {
        throw errors::RuntimeError("Could not stat JSON source: " + source);
    }
    mmap_size_ = sb.st_size;
    
    if (mmap_size_ > security::Limits::Get().max_file_size) {
        throw errors::SecurityError("JSON file exceeds maximum allowed size.");
    }

    if (mmap_size_ > 0) {
        mmap_data_ = (char*)mmap(NULL, mmap_size_, PROT_READ, MAP_PRIVATE, fd_, 0);
        if (mmap_data_ == MAP_FAILED) {
            throw errors::RuntimeError("Failed to memory map JSON source");
        }
    }
    is_open_ = true;
    end_offset_ = mmap_size_;
    
    SkipWhitespace();
    if (pos_ < mmap_size_ && mmap_data_[pos_] == '[') {
        pos_++; // Skip array start
    } else {
        throw errors::RuntimeError("JSON adapter expects a top-level array of objects.");
    }
}

void JsonAdapter::SkipWhitespace() {
    while (pos_ < mmap_size_ && std::isspace(static_cast<unsigned char>(mmap_data_[pos_]))) {
        pos_++;
    }
}

std::string JsonAdapter::ParseString() {
    SkipWhitespace();
    if (pos_ >= mmap_size_ || mmap_data_[pos_] != '"') {
        throw errors::RuntimeError("Expected string in JSON.");
    }
    pos_++; // skip quote
    
    size_t start = pos_;
    bool has_escape = false;
    
    while (pos_ < mmap_size_ && mmap_data_[pos_] != '"') {
        if (mmap_data_[pos_] == '\\') {
            has_escape = true;
            pos_++;
        }
        pos_++;
    }
    
    if (pos_ >= mmap_size_) {
        throw errors::RuntimeError("Unterminated string in JSON.");
    }
    
    std::string result;
    if (!has_escape) {
        result = std::string(mmap_data_ + start, pos_ - start);
    } else {
        size_t p = start;
        while (p < pos_) {
            if (mmap_data_[p] == '\\') {
                p++;
                if (p < pos_) result += mmap_data_[p++];
            } else {
                result += mmap_data_[p++];
            }
        }
    }
    
    pos_++; // skip closing quote
    return result;
}

void JsonAdapter::Match(char expected) {
    SkipWhitespace();
    if (pos_ < mmap_size_ && mmap_data_[pos_] == expected) {
        pos_++;
    } else {
        throw errors::RuntimeError(std::string("Expected '") + expected + "' in JSON.");
    }
}

Row JsonAdapter::ParseObject() {
    Row row;
    Match('{');
    
    SkipWhitespace();
    while (pos_ < mmap_size_ && mmap_data_[pos_] != '}') {
        std::string key = ParseString();
        Match(':');
        
        SkipWhitespace();
        
        if (mmap_data_[pos_] == '"') {
            row[key] = ParseString();
        } else if (std::isdigit(static_cast<unsigned char>(mmap_data_[pos_])) || mmap_data_[pos_] == '-') {
            size_t start = pos_;
            while (pos_ < mmap_size_ && (std::isdigit(static_cast<unsigned char>(mmap_data_[pos_])) || mmap_data_[pos_] == '.' || mmap_data_[pos_] == '-')) {
                pos_++;
            }
            row[key] = std::string(mmap_data_ + start, pos_ - start);
        } else {
            throw errors::RuntimeError("Unsupported JSON value type. MVP only supports strings and numbers.");
        }
        
        SkipWhitespace();
        if (pos_ < mmap_size_ && mmap_data_[pos_] == ',') {
            pos_++;
            SkipWhitespace();
        } else if (pos_ < mmap_size_ && mmap_data_[pos_] != '}') {
            throw errors::RuntimeError("Expected ',' or '}' in JSON object.");
        }
    }
    Match('}');
    return row;
}

bool JsonAdapter::MoveToNextObject() {
    if (!is_open_) return false;
    if (pos_ >= end_offset_) return false;
    
    SkipWhitespace();
    
    if (pos_ >= end_offset_) return false;
    
    if (pos_ < mmap_size_ && mmap_data_[pos_] == ',') {
        pos_++;
        SkipWhitespace();
    }
    
    if (pos_ < mmap_size_ && mmap_data_[pos_] == '{') {
        return true;
    }
    
    if (pos_ < mmap_size_ && mmap_data_[pos_] == ']') {
        return false;
    }
    
    return false; // Reached end or malformed
}

bool JsonAdapter::HasNext() {
    return MoveToNextObject();
}

Row JsonAdapter::Next() {
    if (!HasNext()) {
        throw errors::RuntimeError("No more rows to read.");
    }
    return ParseObject();
}

void JsonAdapter::Close() {
    is_open_ = false;
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

std::vector<std::unique_ptr<IAdapter>> JsonAdapter::Split(size_t num_splits) {
    std::vector<std::unique_ptr<IAdapter>> splits;
    if (mmap_size_ == 0 || num_splits <= 1) return splits;
    
    size_t chunk_size = mmap_size_ / num_splits;
    for (size_t i = 0; i < num_splits; ++i) {
        auto child = std::make_unique<JsonAdapter>();
        child->fd_ = this->fd_;
        child->mmap_data_ = this->mmap_data_;
        child->mmap_size_ = this->mmap_size_;
        child->is_child_ = true;
        child->is_open_ = true;
        size_t start = i * chunk_size;
        size_t end = (i == num_splits - 1) ? mmap_size_ : (i + 1) * chunk_size;
        child->SetChunk(start, end);
        splits.push_back(std::move(child));
    }
    return splits;
}

void JsonAdapter::SetChunk(size_t start, size_t end) {
    end_offset_ = end;
    pos_ = start;
    if (pos_ == 0) {
        SkipWhitespace();
        if (pos_ < mmap_size_ && mmap_data_[pos_] == '[') {
            pos_++;
        }
    } else {
        while (pos_ < mmap_size_) {
            if (mmap_data_[pos_] == '{') {
                size_t prev = pos_ - 1;
                while (prev > 0 && std::isspace(static_cast<unsigned char>(mmap_data_[prev]))) {
                    prev--;
                }
                if (mmap_data_[prev] == ',' || mmap_data_[prev] == '[') {
                    break;
                }
            }
            pos_++;
        }
    }
}

} // namespace json
} // namespace adapters
} // namespace qle
