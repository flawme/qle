#pragma once

#include "adapters/adapter.h"
#include <fstream>
#include <string>

namespace qle {
namespace adapters {
namespace yaml {

class YamlAdapter : public IAdapter {
public:
    YamlAdapter();
    ~YamlAdapter() override;
    
    void Open(const std::string& source) override;
    bool HasNext() override;
    Row Next() override;
    void Close() override;

private:
    std::ifstream file_;
    Row next_row_;
    bool has_next_;
    bool eof_reached_;
    std::string lookahead_line_;

    void FetchNextRow();
    void ProcessLine(const std::string& line, Row& row);
    std::string Trim(const std::string& s);
};

} // namespace yaml
} // namespace adapters
} // namespace qle
