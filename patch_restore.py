import os

with open('src/runtime/runtime.cpp', 'r') as f:
    content = f.read()

restore = """std::vector<std::string> Runtime::ResolveWildcard(const adapters::Row& row) {
    std::vector<std::string> fields;
    fields.reserve(row.size());
    for (const auto& pair : row) {
        fields.push_back(pair.first);
    }
    return fields;
}

void Runtime::SortRows(std::vector<adapters::Row>& rows, const ast::OrderByNode* order_by) {
    const std::string& field = order_by->GetField();
    bool descending = (order_by->GetDirection() == ast::OrderDirection::DESC);

    std::stable_sort(rows.begin(), rows.end(),
        [&field, descending](const adapters::Row& a, const adapters::Row& b) {
            auto it_a = a.find(field);
            auto it_b = b.find(field);
            std::string val_a = (it_a != a.end()) ? it_a->second : "";
            std::string val_b = (it_b != b.end()) ? it_b->second : "";

            try {
                double num_a = std::stod(val_a);
                double num_b = std::stod(val_b);
                return descending ? (num_a > num_b) : (num_a < num_b);
            } catch (...) {
                return descending ? (val_a > val_b) : (val_a < val_b);
            }
        });
}
"""

start_idx = content.find("std::unique_ptr<adapters::IAdapter> Runtime::GetAdapterForSource")

content = content[:start_idx] + restore + "\n" + content[start_idx:]

with open('src/runtime/runtime.cpp', 'w') as f:
    f.write(content)
