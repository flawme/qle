#pragma once

#include "adapters/adapter.h"
#include <fstream>
#include <vector>

namespace qle {
namespace adapters {
namespace csv {

class CsvAdapter : public IAdapter {
public:
    CsvAdapter();
    ~CsvAdapter() override;
    
    void Open(const std::string& source) override;
    bool HasNext() override;
    Row Next() override;
    void Close() override;

private:
    std::ifstream file_;
    std::vector<std::string> headers_;
    std::string next_line_;
    bool has_next_;

    void ReadHeaders();
    void FetchNextLine();
    std::vector<std::string> ParseLine(const std::string& line);
};

} // namespace csv
} // namespace adapters
} // namespace qle
