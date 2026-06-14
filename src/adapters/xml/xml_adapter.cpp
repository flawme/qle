#include "adapters/xml/xml_adapter.h"
#include "errors/errors.h"
#include "security/limits.h"
#include <sys/stat.h>

namespace qle {
namespace adapters {
namespace xml {

XmlAdapter::XmlAdapter() : has_next_(false) {}
XmlAdapter::~XmlAdapter() { Close(); }

void XmlAdapter::Open(const std::string& source) {
    struct stat st;
    if (stat(source.c_str(), &st) == 0) {
        if (static_cast<size_t>(st.st_size) > security::Limits::Get().max_file_size) {
            throw errors::SecurityError("File size exceeds maximum allowed size.");
        }
    }

    file_.open(source);
    if (!file_.is_open()) {
        throw errors::RuntimeError("Could not open XML file: " + source);
    }
    
    FetchNextRow();
}

bool XmlAdapter::HasNext() {
    return has_next_;
}

Row XmlAdapter::Next() {
    Row current_row = next_row_;
    FetchNextRow();
    return current_row;
}

void XmlAdapter::Close() {
    if (file_.is_open()) {
        file_.close();
    }
}

std::string XmlAdapter::ReadNextTag(bool& is_closing, std::string& text_before_tag) {
    text_before_tag.clear();
    char c;
    while (file_.get(c)) {
        if (c == '<') {
            std::string tag;
            while (file_.get(c) && c != '>') {
                tag += c;
            }
            if (!tag.empty() && (tag[0] == '?' || tag[0] == '!')) {
                // Ignore <?xml ... ?> and <!-- ... -->
                continue;
            }
            is_closing = false;
            if (!tag.empty() && tag[0] == '/') {
                is_closing = true;
                tag = tag.substr(1);
            }
            
            // Remove attributes if present, e.g., <col name="id"> -> col
            size_t space_pos = tag.find(' ');
            if (space_pos != std::string::npos) {
                tag = tag.substr(0, space_pos);
            }
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
        tag = ReadNextTag(is_closing, text);
        if (tag.empty()) return; // EOF
        
        if (root_tag_.empty()) {
            if (!is_closing) root_tag_ = tag;
            continue;
        }
        
        if (row_tag_.empty()) {
            if (is_closing) continue; // Skip closing root tag if empty
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
                // Trim leading/trailing whitespace
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
