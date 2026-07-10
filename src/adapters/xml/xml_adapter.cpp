#include "adapters/xml/xml_adapter.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "security/path_validator.h"
#include "utils/mmap_compat.h"
#include <sys/stat.h>
#include <fcntl.h>


namespace qle {
namespace adapters {
namespace xml {

XmlAdapter::XmlAdapter() : fd_(-1), mmap_data_(nullptr), mmap_size_(0), pos_(0), end_offset_(0), is_child_(false), has_next_(false) {}

XmlAdapter::~XmlAdapter() { Close(); }

void XmlAdapter::Open(const std::string& source) {
    security::ValidateSourcePath(source);
    
    fd_ = open(source.c_str(), O_RDONLY);
    if (fd_ < 0) {
        throw errors::RuntimeError("Could not open XML source: " + source);
    }

    struct stat sb;
    if (fstat(fd_, &sb) == -1) {
        throw errors::RuntimeError("Could not stat XML source: " + source);
    }
    mmap_size_ = sb.st_size;
    
    if (mmap_size_ > security::Limits::Get().max_file_size) {
        throw errors::SecurityError("XML file exceeds maximum allowed size.");
    }

    if (mmap_size_ > 0) {
        mmap_data_ = (char*)mmap(NULL, mmap_size_, PROT_READ, MAP_PRIVATE, fd_, 0);
        if (mmap_data_ == MAP_FAILED) {
            throw errors::RuntimeError("Failed to memory map XML source");
        }
    }
    
    end_offset_ = mmap_size_;
    FetchNextRow();
}

bool XmlAdapter::HasNext() {
    return has_next_;
}

Row XmlAdapter::Next() {
    if (!has_next_) {
        throw errors::RuntimeError("No more rows to read.");
    }
    Row current_row = next_row_;
    FetchNextRow();
    return current_row;
}

void XmlAdapter::Close() {
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

std::vector<std::unique_ptr<IAdapter>> XmlAdapter::Split(size_t num_splits) {
    std::vector<std::unique_ptr<IAdapter>> splits;
    if (mmap_size_ == 0 || num_splits <= 1) return splits;
    
    size_t chunk_size = mmap_size_ / num_splits;
    for (size_t i = 0; i < num_splits; ++i) {
        auto child = std::make_unique<XmlAdapter>();
        child->fd_ = this->fd_;
        child->mmap_data_ = this->mmap_data_;
        child->mmap_size_ = this->mmap_size_;
        child->is_child_ = true;
        child->row_tag_ = this->row_tag_;
        child->root_tag_ = this->root_tag_;
        
        size_t start = i * chunk_size;
        size_t end = (i == num_splits - 1) ? mmap_size_ : (i + 1) * chunk_size;
        child->SetChunk(start, end);
        splits.push_back(std::move(child));
    }
    return splits;
}

void XmlAdapter::SetChunk(size_t start, size_t end) {
    end_offset_ = end;
    pos_ = start;
    if (pos_ == 0) {
        FetchNextRow();
    } else {
        if (!row_tag_.empty()) {
            while (pos_ < mmap_size_) {
                if (mmap_data_[pos_] == '<') {
                    bool match = true;
                    for (size_t j = 0; j < row_tag_.size(); ++j) {
                        if (pos_ + 1 + j >= mmap_size_ || mmap_data_[pos_ + 1 + j] != row_tag_[j]) {
                            match = false;
                            break;
                        }
                    }
                    if (match) {
                        char after = pos_ + 1 + row_tag_.size() < mmap_size_ ? mmap_data_[pos_ + 1 + row_tag_.size()] : '\0';
                        if (after == '>' || after == ' ' || after == '/') {
                            break;
                        }
                    }
                }
                pos_++;
            }
        }
        FetchNextRow();
    }
}

std::string XmlAdapter::ReadNextTag(bool& is_closing, std::string& text_before_tag) {
    text_before_tag.clear();
    while (pos_ < mmap_size_) {
        char c = mmap_data_[pos_++];
        if (c == '<') {
            std::string tag;
            while (pos_ < mmap_size_ && mmap_data_[pos_] != '>') {
                tag += mmap_data_[pos_++];
            }
            if (pos_ < mmap_size_ && mmap_data_[pos_] == '>') pos_++;
            
            if (!tag.empty() && (tag[0] == '?' || tag[0] == '!')) continue;
            
            is_closing = false;
            if (!tag.empty() && tag[0] == '/') {
                is_closing = true;
                tag = tag.substr(1);
            }
            
            size_t space_pos = tag.find(' ');
            if (space_pos != std::string::npos) tag = tag.substr(0, space_pos);
            return tag;
        } else {
            text_before_tag += c;
        }
    }
    return "";
}

void XmlAdapter::FetchNextRow() {
    has_next_ = false;
    next_row_.clear();
    
    bool is_closing = false;
    std::string text;
    std::string tag;
    
    while (true) {
        if (pos_ >= end_offset_) return;
        
        tag = ReadNextTag(is_closing, text);
        if (tag.empty()) return; // EOF
        
        if (root_tag_.empty()) {
            if (!is_closing) root_tag_ = tag;
            continue;
        }
        
        if (row_tag_.empty()) {
            if (is_closing) continue; 
            row_tag_ = tag;
            break;
        } else if (tag == row_tag_ && !is_closing) {
            break;
        }
    }
    
    while (true) {
        tag = ReadNextTag(is_closing, text);
        if (tag.empty()) break; // EOF
        
        if (is_closing && tag == row_tag_) {
            has_next_ = true;
            return;
        }
        
        if (!is_closing) {
            std::string col_name = tag;
            std::string val_text;
            std::string end_tag = ReadNextTag(is_closing, val_text);
            
            if (is_closing && end_tag == col_name) {
                size_t first = val_text.find_first_not_of(" \t\r\n");
                if (first != std::string::npos) {
                    size_t last = val_text.find_last_not_of(" \t\r\n");
                    val_text = val_text.substr(first, last - first + 1);
                } else {
                    val_text = "";
                }
                next_row_[col_name] = val_text;
            } else {
                throw errors::RuntimeError("Malformed XML or nested columns not supported in flat parser");
            }
        }
    }
}

} // namespace xml
} // namespace adapters
} // namespace qle
