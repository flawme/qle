#include "utils/formatter.h"
#include <iostream>
#include <algorithm>
#include <iomanip>

#include "errors/errors.h"

namespace qle {
namespace utils {

void Formatter::SetOutputFile(const std::string& filename) {
    if (file_stream_.is_open()) file_stream_.close();
    file_stream_.open(filename);
    if (!file_stream_.is_open()) {
        throw errors::RuntimeError("Could not open output file: " + filename);
    }
    out_stream_ = &file_stream_;
}

void Formatter::PrintHeader(const std::vector<std::string>& fields, OutputFormat format) {
    if (format == OutputFormat::TABLE) {
        headers_ = fields;
    } else if (format == OutputFormat::CSV || format == OutputFormat::TSV) {
        std::string delim = (format == OutputFormat::CSV) ? "," : "\t";
        for (size_t i = 0; i < fields.size(); ++i) {
            (*out_stream_) << fields[i] << (i + 1 == fields.size() ? "" : delim);
        }
        (*out_stream_) << "\n";
    } else if (format == OutputFormat::JSON) {
        (*out_stream_) << "[\n";
        first_json_row_ = true;
    }
}

void Formatter::PrintRow(const adapters::Row& row, const std::vector<std::string>& fields, OutputFormat format) {
    if (format == OutputFormat::TABLE) {
        std::vector<std::string> row_data;
        for (const auto& field : fields) {
            auto it = row.find(field);
            row_data.push_back(it != row.end() ? it->second : "");
        }
        buffered_rows_.push_back(std::move(row_data));
    } else if (format == OutputFormat::CSV || format == OutputFormat::TSV) {
        std::string delim = (format == OutputFormat::CSV) ? "," : "\t";
        for (size_t i = 0; i < fields.size(); ++i) {
            auto it = row.find(fields[i]);
            (*out_stream_) << (it != row.end() ? it->second : "") << (i + 1 == fields.size() ? "" : delim);
        }
        (*out_stream_) << "\n";
    } else if (format == OutputFormat::JSON) {
        if (!first_json_row_) {
            (*out_stream_) << ",\n";
        }
        first_json_row_ = false;
        (*out_stream_) << "  {\n";
        for (size_t i = 0; i < fields.size(); ++i) {
            auto it = row.find(fields[i]);
            std::string val = (it != row.end() ? it->second : "");
            (*out_stream_) << "    \"" << fields[i] << "\": \"" << val << "\"" << (i + 1 == fields.size() ? "" : ", ") << "\n";
        }
        (*out_stream_) << "  }";
    }
}

void Formatter::Flush(OutputFormat format) {
    if (format == OutputFormat::TABLE) {
        if (headers_.empty()) return;
        
        std::vector<size_t> col_widths(headers_.size(), 0);
        for (size_t i = 0; i < headers_.size(); ++i) {
            col_widths[i] = headers_[i].length();
        }
        
        for (const auto& row : buffered_rows_) {
            for (size_t i = 0; i < row.size() && i < col_widths.size(); ++i) {
                col_widths[i] = std::max(col_widths[i], row[i].length());
            }
        }
        
        auto print_sep = [&]() {
            (*out_stream_) << "+";
            for (size_t w : col_widths) (*out_stream_) << std::string(w + 2, '-') << "+";
            (*out_stream_) << "\n";
        };
        
        print_sep();
        (*out_stream_) << "|";
        for (size_t i = 0; i < headers_.size(); ++i) {
            (*out_stream_) << " " << std::left << std::setw(col_widths[i]) << headers_[i] << " |";
        }
        (*out_stream_) << "\n";
        print_sep();
        
        for (const auto& row : buffered_rows_) {
            (*out_stream_) << "|";
            for (size_t i = 0; i < row.size() && i < col_widths.size(); ++i) {
                (*out_stream_) << " " << std::left << std::setw(col_widths[i]) << row[i] << " |";
            }
            (*out_stream_) << "\n";
        }
        print_sep();
        
        buffered_rows_.clear();
        headers_.clear();
    } else if (format == OutputFormat::JSON) {
        (*out_stream_) << "\n]\n";
    }
    out_stream_->flush();
}

} // namespace utils
} // namespace qle
