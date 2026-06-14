#include <unordered_map>
#include <string>
#include <cstdlib>
#include <limits>
#include <map>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <functional>
#include "adapters/sqlite/sqlite_adapter.h"
#include "runtime/runtime.h"
#include "adapters/csv/csv_adapter.h"
#include "adapters/json/json_adapter.h"
#include "adapters/yaml/yaml_adapter.h"
#include "adapters/xml/xml_adapter.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "logger/logger.h"
#include "utils/suggestions.h"
#include <iostream>

namespace qle {
namespace runtime {

namespace {
class MemoryAdapter : public adapters::IAdapter {
public:
    explicit MemoryAdapter(std::vector<adapters::Row> rows) : rows_(std::move(rows)), index_(0) {}
    void Open(const std::string& source) override {}
    void Close() override {}
    bool HasNext() override { return index_ < rows_.size(); }
    adapters::Row Next() override { return rows_[index_++]; }
private:
    std::vector<adapters::Row> rows_;
    size_t index_;
};
}

Runtime::Runtime()
    : rows_processed_(0), format_(utils::OutputFormat::CSV) {}

void Runtime::SetFormat(utils::OutputFormat format) {
    format_ = format;
}

std::vector<adapters::Row> Runtime::ExecuteToMemory(const ast::QueryNode* query) {
    execute_to_memory_ = true;
    memory_results_.clear();
    Execute(query);
    return std::move(memory_results_);
}

void Runtime::Execute(const ast::QueryNode* query) {
    Debug::DebugLog("Runtime starting execution");
    rows_processed_ = 0;
    start_time_ = std::chrono::steady_clock::now();

    const ast::SourceNode* source_node = query->GetSource();
    std::unique_ptr<adapters::IAdapter> adapter;
    
    if (source_node->IsSubquery()) {
        Runtime sub_rt;
        auto rows = sub_rt.ExecuteToMemory(source_node->GetSubquery());
        adapter = std::make_unique<MemoryAdapter>(std::move(rows));
    } else {
        std::string source_name = source_node->GetSourceName();
        adapter = GetAdapterForSource(source_name);
        adapter->Open(source_name);
    }

    const ast::WhereNode* where_node = query->GetWhere();
    const std::vector<std::unique_ptr<ast::JoinNode>>& join_nodes = query->GetJoins();
    const ast::SelectNode* select_node = query->GetSelect();
    const ast::OrderByNode* order_by = query->GetOrderBy();
    const ast::GroupByNode* group_by = query->GetGroupBy();
    size_t limit = query->GetLimit();

    if (group_by || HasAggregate(select_node)) {
        ExecuteWithGroupBy(*adapter, select_node, where_node, group_by, order_by, limit);
    } else if (order_by) {
        ExecuteWithOrderBy(*adapter, select_node, where_node, order_by, limit);
    } else {
        ExecuteStreaming(*adapter, select_node, where_node, join_nodes, limit);
    }

    adapter->Close();
    if (!execute_to_memory_) {
        formatter_.Flush(format_);
    }
    Debug::DebugLog("Execution finished. Rows processed: " + std::to_string(rows_processed_));
}

std::vector<std::string> Runtime::ResolveWildcard(const adapters::Row& row) {
    std::vector<std::string> fields;
    fields.reserve(row.size());
    for (const auto& pair : row) {
        fields.push_back(pair.first);
    }
    return fields;
}

std::unique_ptr<adapters::IAdapter> Runtime::GetAdapterForSource(
    const std::string& source) {
    if (source.size() > 4 &&
        source.substr(source.size() - 4) == ".csv") {
        return std::make_unique<adapters::csv::CsvAdapter>();
    }
    if (source.size() > 5 &&
        (source.substr(source.size() - 5) == ".yaml" || source.substr(source.size() - 4) == ".yml")) {
        return std::make_unique<adapters::yaml::YamlAdapter>();
    }
    if (source.size() > 4 &&
        source.substr(source.size() - 4) == ".xml") {
        return std::make_unique<adapters::xml::XmlAdapter>();
    }
    if (source.size() > 5 && source.substr(source.size() - 5) == ".json") {
        return std::make_unique<adapters::json::JsonAdapter>();
    }
    if (source.find(".sqlite") != std::string::npos || source.find(".db") != std::string::npos) {
        return std::make_unique<adapters::SQLiteAdapter>();
    }
    throw errors::RuntimeError(
        "Unsupported source format or adapter not found for: " + source);
}

void Runtime::CheckTimeout() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
    if (static_cast<size_t>(duration.count()) > security::Limits::Get().max_execution_time_ms) {
        throw errors::SecurityError("Maximum execution time exceeded (" + std::to_string(security::Limits::Get().max_execution_time_ms) + " ms)");
    }
}

} // namespace runtime
} // namespace qle
