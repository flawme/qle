import os

runtime_cpp = "src/runtime/runtime.cpp"
with open(runtime_cpp, "r") as f:
    content = f.read()

start_sig = "void Runtime::ExecuteStreaming(adapters::IAdapter& adapter,"
end_sig = "void Runtime::ExecuteWithOrderBy"

start_idx = content.find(start_sig)
end_idx = content.find(end_sig)

if start_idx == -1 or end_idx == -1:
    print("Could not find method.")
    exit(1)

new_method = """void Runtime::ExecuteStreaming(adapters::IAdapter& adapter,
                               const ast::SelectNode* select_node,
                               const ast::WhereNode* where_node, const ast::JoinNode* join_node, size_t limit) {
    bool header_printed = false;
    size_t output_count = 0;
    std::vector<std::string> field_names;

    auto process_row = [&](adapters::Row& row) {
        bool include_row = true;
        if (where_node) {
            include_row = EvaluateCondition(where_node->GetCondition(), row);
        }

        if (include_row) {
            if (select_node->IsWildcard() && !header_printed) {
                field_names = ResolveWildcard(row);
            } else if (!select_node->IsWildcard() && !header_printed) {
                for (const auto& expr : select_node->GetFields()) {
                    field_names.push_back(FormatExpression(expr.get()));
                }
            }
            if (!header_printed) {
                formatter_.PrintHeader(field_names, format_);
                header_printed = true;
            }
            
            if (select_node->IsWildcard()) {
                formatter_.PrintRow(row, field_names, format_);
            } else {
                adapters::Row out_row;
                for (size_t i = 0; i < select_node->GetFields().size(); ++i) {
                    out_row[field_names[i]] = EvaluateExpression(select_node->GetFields()[i].get(), row);
                }
                formatter_.PrintRow(out_row, field_names, format_);
            }
            output_count++;
        }
    };

    while (adapter.HasNext()) {
        if (rows_processed_ >= security::Limits::Get().max_rows_processed) {
            throw errors::SecurityError("Maximum row limit exceeded.");
        }
        adapters::Row primary_row = adapter.Next();
        rows_processed_++;
        if (rows_processed_ % 10000 == 0) CheckTimeout();

        if (join_node) {
            auto joined_adapter = GetAdapterForSource(join_node->GetSource());
            joined_adapter->Open(join_node->GetSource());
            while (joined_adapter->HasNext()) {
                adapters::Row joined_row = joined_adapter->Next();
                adapters::Row row = primary_row;
                for (const auto& kv : joined_row) {
                    row[kv.first] = kv.second;
                }
                
                if (EvaluateCondition(join_node->GetCondition(), row)) {
                    process_row(row);
                }
                if (limit > 0 && output_count >= limit) break;
            }
            joined_adapter->Close();
        } else {
            process_row(primary_row);
        }
        if (limit > 0 && output_count >= limit) break;
    }

    if (!header_printed && !select_node->IsWildcard()) {
        for (const auto& expr : select_node->GetFields()) {
            field_names.push_back(FormatExpression(expr.get()));
        }
        formatter_.PrintHeader(field_names, format_);
    }
}

"""

new_content = content[:start_idx] + new_method + content[end_idx:]

with open(runtime_cpp, "w") as f:
    f.write(new_content)

