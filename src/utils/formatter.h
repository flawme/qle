#pragma once

#include "adapters/adapter.h"
#include <vector>
#include <string>

#include <fstream>
#include <ostream>
#include <iostream>

namespace qle {
namespace utils {

enum class OutputFormat {
    TABLE,
    CSV,
    TSV,
    JSON
};

class Formatter {
public:
    Formatter() : out_stream_(&std::cout) {}
    ~Formatter() { if (file_stream_.is_open()) file_stream_.close(); }

    void SetOutputFile(const std::string& filename);
    void PrintHeader(const std::vector<std::string>& fields, OutputFormat format);
    void PrintRow(const adapters::Row& row, const std::vector<std::string>& fields, OutputFormat format);
    void Flush(OutputFormat format);

private:
    std::vector<std::vector<std::string>> buffered_rows_;
    std::vector<std::string> headers_;
    bool first_json_row_ = true;
    std::ostream* out_stream_;
    std::ofstream file_stream_;
};

} // namespace utils
} // namespace qle
