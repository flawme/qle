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
            process_row(primary_row);
        }
        if (limit > 0 && output_count >= limit) break;
    }

