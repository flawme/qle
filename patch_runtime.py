import sys
with open('src/runtime/runtime.cpp', 'r') as f:
    content = f.read()

target1 = """    std::unordered_multimap<std::string, adapters::Row> join_hash_map;
    std::vector<adapters::Row> joined_rows_fallback;
    bool is_simple_equi_join = false;
    std::string primary_key_name;"""

replacement1 = """    std::unordered_multimap<std::string, adapters::Row> join_hash_map;
    std::vector<adapters::Row> joined_rows_fallback;
    bool is_simple_equi_join = false;
    std::string primary_key_name;
    size_t estimated_memory = 0;"""

target2 = """            if (is_simple_equi_join) {
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
        }"""

replacement2 = """            if (is_simple_equi_join) {
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
                
                size_t row_memory = key_val.capacity() + 64;
                for (const auto& kv : joined_row) {
                    row_memory += kv.first.capacity() + kv.second.capacity() + 64;
                }
                estimated_memory += row_memory;
                if (estimated_memory > security::Limits::Get().max_memory_usage) {
                    throw errors::SecurityError("Maximum memory limit exceeded during join.");
                }
                
                join_hash_map.insert({key_val, std::move(joined_row)});
            } else {
                size_t row_memory = 64;
                for (const auto& kv : joined_row) {
                    row_memory += kv.first.capacity() + kv.second.capacity() + 64;
                }
                estimated_memory += row_memory;
                if (estimated_memory > security::Limits::Get().max_memory_usage) {
                    throw errors::SecurityError("Maximum memory limit exceeded during join.");
                }
                
                joined_rows_fallback.push_back(std::move(joined_row));
            }
        }"""

if target1 in content and target2 in content:
    content = content.replace(target1, replacement1)
    content = content.replace(target2, replacement2)
    with open('src/runtime/runtime.cpp', 'w') as f:
        f.write(content)
    print("Success")
else:
    print("Failed to find targets")
    if target1 not in content: print("target1 not found")
    if target2 not in content: print("target2 not found")
