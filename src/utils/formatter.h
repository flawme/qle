#pragma once

#include "adapters/adapter.h"
#include <vector>
#include <string>

namespace qle {
namespace utils {

enum class OutputFormat {
    TABLE,
    CSV,
    JSON
};

class Formatter {
public:
    void PrintHeader(const std::vector<std::string>& fields, OutputFormat format);
    void PrintRow(const adapters::Row& row, const std::vector<std::string>& fields, OutputFormat format);
    void Flush(OutputFormat format);

private:
    std::vector<std::vector<std::string>> buffered_rows_;
    std::vector<std::string> headers_;
    bool first_json_row_ = true;
};

} // namespace utils
} // namespace qle
