import re

with open('src/runtime/runtime.cpp', 'r') as f:
    content = f.read()

# Fix JOIN limit bypass
new_join_logic = """
        if (join_node) {
            auto joined_adapter = GetAdapterForSource(join_node->GetSource());
            joined_adapter->Open(join_node->GetSource());
            while (joined_adapter->HasNext()) {
                if (rows_processed_ >= security::Limits::Get().max_rows_processed) {
                    throw errors::SecurityError("Maximum row limit exceeded.");
                }
                adapters::Row joined_row = joined_adapter->Next();
                rows_processed_++;
                if (rows_processed_ % 10000 == 0) CheckTimeout();
                
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
"""

content = content.replace("""
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
""".strip('\n'), new_join_logic.strip('\n'))

with open('src/runtime/runtime.cpp', 'w') as f:
    f.write(content)
