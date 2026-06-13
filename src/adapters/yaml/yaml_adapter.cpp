#include "adapters/yaml/yaml_adapter.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "security/path_validator.h"

namespace qle {
namespace adapters {
namespace yaml {

YamlAdapter::YamlAdapter() : has_next_(false), eof_reached_(false) {}

YamlAdapter::~YamlAdapter() {
    Close();
}

void YamlAdapter::Open(const std::string& source) {
    security::ValidateSourcePath(source);
    
    file_.open(source);
    if (!file_.is_open()) {
        throw errors::RuntimeError("Could not open YAML source: " + source);
    }

    // Check file size limit
    file_.seekg(0, std::ios::end);
    size_t size = file_.tellg();
    file_.seekg(0, std::ios::beg);
    
    if (size > security::Limits::Get().max_file_size) {
        throw errors::SecurityError("YAML file exceeds maximum allowed size.");
    }

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
    if (file_.is_open()) {
        file_.close();
    }
}

void YamlAdapter::FetchNextRow() {
    has_next_ = false;
    if (eof_reached_ && lookahead_line_.empty()) {
        return;
    }

    Row row;
    bool row_started = false;

    // Process lookahead line first if we have one
    if (!lookahead_line_.empty()) {
        ProcessLine(lookahead_line_, row);
        lookahead_line_.clear();
        row_started = true;
    }

    std::string line;
    while (std::getline(file_, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue; // Skip empty and comment lines
        }

        if (trimmed.rfind("- ", 0) == 0 || trimmed == "-") {
            if (row_started) {
                // We reached the start of the next item. Save line for next row.
                lookahead_line_ = trimmed;
                has_next_ = true;
                next_row_ = row;
                return;
            } else {
                // First item
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
        content = content.substr(2); // remove "- "
    } else if (content == "-") {
        return; // nothing to process here
    }

    // Now look for ':'
    size_t colon_pos = content.find(':');
    if (colon_pos != std::string::npos) {
        std::string key = Trim(content.substr(0, colon_pos));
        std::string value = Trim(content.substr(colon_pos + 1));
        
        // Strip basic quotes from value
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
