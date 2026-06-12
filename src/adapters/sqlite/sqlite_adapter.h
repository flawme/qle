#pragma once

#include "adapters/adapter.h"
#include <string>

namespace qle {
namespace adapters {

class SQLiteAdapter : public IAdapter {
public:
    SQLiteAdapter();
    ~SQLiteAdapter() override;

    void Open(const std::string& source) override;
    bool HasNext() override;
    Row Next() override;
    void Close() override;

private:
    void* db_;
    void* stmt_;
    bool has_next_;
    Row current_row_;

    void FetchNext();
};

} // namespace adapters
} // namespace qle
