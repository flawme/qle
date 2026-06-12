#include "utils/formatter.h"
#include <iostream>
#include <algorithm>
#include <iomanip>

namespace qle {
namespace utils {

void Formatter::PrintHeader(const std::vector<std::string>& fields, OutputFormat format) {
    if (format == OutputFormat::TABLE) {
        headers_ = fields;
    } else if (format == OutputFormat::CSV) {
        for (size_t i = 0; i < fields.size(); ++i) {
            std::cout << fields[i] << (i + 1 == fields.size() ? "" : ",");
        }
        std::cout << std::endl;
    } else if (format == OutputFormat::JSON) {
        std::cout << "[" << std::endl;
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
    } else if (format == OutputFormat::CSV) {
        for (size_t i = 0; i < fields.size(); ++i) {
            auto it = row.find(fields[i]);
            std::cout << (it != row.end() ? it->second : "") << (i + 1 == fields.size() ? "" : ",");
        }
        std::cout << std::endl;
    } else if (format == OutputFormat::JSON) {
        if (!first_json_row_) {
            std::cout << "," << std::endl;
        }
        first_json_row_ = false;
        std::cout << "  {" << std::endl;
        for (size_t i = 0; i < fields.size(); ++i) {
            auto it = row.find(fields[i]);
            std::string val = (it != row.end() ? it->second : "");
            std::cout << "    \"" << fields[i] << "\": \"" << val << "\"" << (i + 1 == fields.size() ? "" : ", ") << std::endl;
        }
        std::cout << "  }";
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
            std::cout << "+";
            for (size_t w : col_widths) std::cout << std::string(w + 2, '-') << "+";
            std::cout << std::endl;
        };
        
        print_sep();
        std::cout << "|";
        for (size_t i = 0; i < headers_.size(); ++i) {
            std::cout << " " << std::left << std::setw(col_widths[i]) << headers_[i] << " |";
        }
        std::cout << std::endl;
        print_sep();
        
        for (const auto& row : buffered_rows_) {
            std::cout << "|";
            for (size_t i = 0; i < row.size() && i < col_widths.size(); ++i) {
                std::cout << " " << std::left << std::setw(col_widths[i]) << row[i] << " |";
            }
            std::cout << std::endl;
        }
        print_sep();
        
        buffered_rows_.clear();
        headers_.clear();
    } else if (format == OutputFormat::JSON) {
        std::cout << std::endl << "]" << std::endl;
    }
}

} // namespace utils
} // namespace qle
