#include "adapters/json/json_adapter.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "security/path_validator.h"
#include <fstream>
#include <sstream>
#include <cctype>

namespace qle {
namespace adapters {
namespace json {

JsonAdapter::JsonAdapter() : pos_(0), is_open_(false) {}

JsonAdapter::~JsonAdapter() {
    Close();
}

void JsonAdapter::Open(const std::string& source) {
    security::ValidateSourcePath(source);
    
    std::ifstream file(source);
    if (!file.is_open()) {
        throw errors::RuntimeError("Could not open JSON source: " + source);
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (size > security::Limits::Get().max_file_size) {
        throw errors::SecurityError("JSON file exceeds maximum allowed size.");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    content_ = buffer.str();
    is_open_ = true;
    
    SkipWhitespace();
    if (pos_ < content_.size() && content_[pos_] == '[') {
        pos_++; // Skip array start
    } else {
        throw errors::RuntimeError("JSON adapter expects a top-level array of objects.");
    }
}

void JsonAdapter::SkipWhitespace() {
    while (pos_ < content_.size() && std::isspace(content_[pos_])) {
        pos_++;
    }
}

std::string JsonAdapter::ParseString() {
    SkipWhitespace();
    if (pos_ >= content_.size() || content_[pos_] != '"') {
        throw errors::RuntimeError("Expected string in JSON.");
    }
    pos_++; // skip quote
    
    std::string result;
    while (pos_ < content_.size() && content_[pos_] != '"') {
        if (content_[pos_] == '\\') {
            pos_++; // basic escape handling
            if (pos_ < content_.size()) {
                result += content_[pos_++];
            }
        } else {
            result += content_[pos_++];
        }
    }
    
    if (pos_ >= content_.size()) {
        throw errors::RuntimeError("Unterminated string in JSON.");
    }
    pos_++; // skip closing quote
    return result;
}

void JsonAdapter::Match(char expected) {
    SkipWhitespace();
    if (pos_ < content_.size() && content_[pos_] == expected) {
        pos_++;
    } else {
        throw errors::RuntimeError(std::string("Expected '") + expected + "' in JSON.");
    }
}

Row JsonAdapter::ParseObject() {
    Row row;
    Match('{');
    
    SkipWhitespace();
    while (pos_ < content_.size() && content_[pos_] != '}') {
        std::string key = ParseString();
        Match(':');
        
        SkipWhitespace();
        std::string value;
        
        // For MVP, we parse strings and numbers as strings
        if (content_[pos_] == '"') {
            value = ParseString();
        } else if (std::isdigit(content_[pos_]) || content_[pos_] == '-') {
            while (pos_ < content_.size() && (std::isdigit(content_[pos_]) || content_[pos_] == '.' || content_[pos_] == '-')) {
                value += content_[pos_++];
            }
        } else {
            throw errors::RuntimeError("Unsupported JSON value type. MVP only supports strings and numbers.");
        }
        
        row[key] = value;
        
        SkipWhitespace();
        if (content_[pos_] == ',') {
            pos_++;
            SkipWhitespace();
        } else if (content_[pos_] != '}') {
            throw errors::RuntimeError("Expected ',' or '}' in JSON object.");
        }
    }
    Match('}');
    return row;
}

bool JsonAdapter::MoveToNextObject() {
    if (!is_open_) return false;
    
    SkipWhitespace();
    
    if (pos_ < content_.size() && content_[pos_] == ',') {
        pos_++;
        SkipWhitespace();
    }
    
    if (pos_ < content_.size() && content_[pos_] == '{') {
        return true;
    }
    
    if (pos_ < content_.size() && content_[pos_] == ']') {
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
    content_.clear();
}

} // namespace json
} // namespace adapters
} // namespace qle
