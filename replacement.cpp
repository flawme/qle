    std::unordered_multimap<std::string, adapters::Row> join_hash_map;
    std::vector<adapters::Row> joined_rows_fallback;
    bool is_simple_equi_join = false;
    std::string primary_key_name;

    if (join_node) {
        auto joined_adapter = GetAdapterForSource(join_node->GetSource());
        joined_adapter->Open(join_node->GetSource());

        const ast::ExpressionNode* cond = join_node->GetCondition();
        std::string left_id, right_id;

        if (cond->GetToken().type == lexer::TokenType::EQUALS && cond->GetLeft()->IsLiteral() && cond->GetRight()->IsLiteral()) {
            if (cond->GetLeft()->GetToken().type == lexer::TokenType::IDENTIFIER && cond->GetRight()->GetToken().type == lexer::TokenType::IDENTIFIER) {
                is_simple_equi_join = true;
                left_id = cond->GetLeft()->GetToken().value;
                right_id = cond->GetRight()->GetToken().value;
            }
        }

        std::string secondary_key_name;

        while (joined_adapter->HasNext()) {
            if (rows_processed_ >= security::Limits::Get().max_rows_processed) {
                throw errors::SecurityError("Maximum row limit exceeded.");
            }
            adapters::Row joined_row = joined_adapter->Next();
            rows_processed_++;
            if (rows_processed_ % 10000 == 0) CheckTimeout();

            if (is_simple_equi_join) {
                if (secondary_key_name.empty()) {
                    if (joined_row.find(left_id) != joined_row.end()) {
                        secondary_key_name = left_id;
                        primary_key_name = right_id;
                    } else if (joined_row.find(right_id) != joined_row.end()) {
                        secondary_key_name = right_id;
                        primary_key_name = left_id;
                    } else {
                        secondary_key_name = left_id;
                        primary_key_name = right_id;
                    }
                }
                std::string key_val;
                auto it = joined_row.find(secondary_key_name);
                if (it != joined_row.end()) {
                    key_val = it->second;
                }
                join_hash_map.insert({key_val, std::move(joined_row)});
            } else {
                joined_rows_fallback.push_back(std::move(joined_row));
            }
        }
        joined_adapter->Close();
    }

    while (adapter.HasNext()) {
        if (rows_processed_ >= security::Limits::Get().max_rows_processed) {
            throw errors::SecurityError("Maximum row limit exceeded.");
        }
        adapters::Row primary_row = adapter.Next();
        rows_processed_++;
        if (rows_processed_ % 10000 == 0) CheckTimeout();

        if (join_node) {
            if (is_simple_equi_join) {
                std::string lookup_val;
                auto it = primary_row.find(primary_key_name);
                if (it != primary_row.end()) {
                    lookup_val = it->second;
                }

                auto range = join_hash_map.equal_range(lookup_val);
                for (auto hash_it = range.first; hash_it != range.second; ++hash_it) {
                    adapters::Row row = primary_row;
                    for (const auto& kv : hash_it->second) {
                        row[kv.first] = kv.second;
                    }
                    if (EvaluateCondition(join_node->GetCondition(), row)) {
                        process_row(row);
                    }
                    if (limit > 0 && output_count >= limit) break;
                }
            } else {
                for (const auto& joined_row : joined_rows_fallback) {
                    adapters::Row row = primary_row;
                    for (const auto& kv : joined_row) {
                        row[kv.first] = kv.second;
                    }
                    if (EvaluateCondition(join_node->GetCondition(), row)) {
                        process_row(row);
                    }
                    if (limit > 0 && output_count >= limit) break;
                }
            }
        } else {
            process_row(primary_row);
        }
        if (limit > 0 && output_count >= limit) break;
    }
