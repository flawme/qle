#pragma once

#include "adapters/adapter.h"
#include <fstream>
#include <string>

namespace qle {
namespace adapters {
namespace xml {

class XmlAdapter : public IAdapter {
public:
    XmlAdapter();
    ~XmlAdapter() override;
    
    void Open(const std::string& source) override;
    bool HasNext() override;
    Row Next() override;
    void Close() override;

private:
    std::ifstream file_;
    bool has_next_;
    Row next_row_;
    std::string root_tag_;
    std::string row_tag_;

    void FetchNextRow();
    std::string ReadNextTag(bool& is_closing, std::string& text_before_tag);
};

} // namespace xml
} // namespace adapters
} // namespace qle
