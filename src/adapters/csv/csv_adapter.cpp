#include "adapters/csv/csv_adapter.h"
#include "errors/errors.h"
#include "security/limits.h"
#include <sstream>

namespace qle {
namespace adapters {
namespace csv {

CsvAdapter::CsvAdapter() : has_next_(false) {}

CsvAdapter::~CsvAdapter() {
    Close();
}

void CsvAdapter::Open(const std::string& source) {
    // Basic path traversal protection
    if (source.find("..") != std::string::npos || source.find('/') != std::string::npos) {
        throw errors::SecurityError("Path traversal or nested directories not allowed in MVP.");
    }
    
    file_.open(source);
    if (!file_.is_open()) {
        throw errors::RuntimeError("Could not open CSV source: " + source);
    }

    // Check file size limit
    file_.seekg(0, std::ios::end);
    size_t size = file_.tellg();
    file_.seekg(0, std::ios::beg);
    
    if (size > security::Limits::Get().max_file_size) {
        throw errors::SecurityError("CSV file exceeds maximum allowed size.");
    }

    ReadHeaders();
    FetchNextLine();
}

void CsvAdapter::ReadHeaders() {
    std::string line;
    if (std::getline(file_, line)) {
        headers_ = ParseLine(line);
    }
}

void CsvAdapter::FetchNextLine() {
    if (std::getline(file_, next_line_)) {
        has_next_ = true;
    } else {
        has_next_ = false;
    }
}

bool CsvAdapter::HasNext() {
    return has_next_;
}

Row CsvAdapter::Next() {
    if (!has_next_) {
        throw errors::RuntimeError("No more rows to read.");
    }

    std::vector<std::string> values = ParseLine(next_line_);
    Row row;
    
    size_t count = std::min(headers_.size(), values.size());
    for (size_t i = 0; i < count; ++i) {
        row[headers_[i]] = values[i];
    }
    
    FetchNextLine();
    return row;
}

void CsvAdapter::Close() {
    if (file_.is_open()) {
        file_.close();
    }
}

std::vector<std::string> CsvAdapter::ParseLine(const std::string& line) {
    std::vector<std::string> result;
    std::stringstream ss(line);
    std::string cell;
    
    // Simplistic CSV parsing for V1.
    while (std::getline(ss, cell, ',')) {
        if (!cell.empty() && cell.back() == '\r') {
            cell.pop_back();
        }
        // Strip basic quotes if present
        if (cell.size() >= 2 && cell.front() == '"' && cell.back() == '"') {
            cell = cell.substr(1, cell.size() - 2);
        }
        result.push_back(cell);
    }
    return result;
}

} // namespace csv
} // namespace adapters
} // namespace qle
