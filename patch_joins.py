import sys
import os

def patch_file(filepath, targets, replacements):
    with open(filepath, "r") as f:
        content = f.read()
    
    for t, r in zip(targets, replacements):
        if t in content:
            content = content.replace(t, r)
        else:
            print(f"Target not found in {filepath}: {t[:30]}...")
            return False

    with open(filepath, "w") as f:
        f.write(content)
    print(f"Patched {filepath} successfully")
    return True

# 1. Update src/ast/ast.h
target_ast_h_1 = """              std::unique_ptr<JoinNode> join_clause,
              std::unique_ptr<WhereNode> where_clause,"""
repl_ast_h_1 = """              std::vector<std::unique_ptr<JoinNode>> join_clauses,
              std::unique_ptr<WhereNode> where_clause,"""

target_ast_h_2 = """    const JoinNode* GetJoin() const { return join_clause_.get(); }"""
repl_ast_h_2 = """    const std::vector<std::unique_ptr<JoinNode>>& GetJoins() const { return join_clauses_; }"""

target_ast_h_3 = """    std::unique_ptr<JoinNode> join_clause_;"""
repl_ast_h_3 = """    std::vector<std::unique_ptr<JoinNode>> join_clauses_;"""

patch_file("src/ast/ast.h", [target_ast_h_1, target_ast_h_2, target_ast_h_3], [repl_ast_h_1, repl_ast_h_2, repl_ast_h_3])

# 2. Update src/ast/ast.cpp
target_ast_cpp = """QueryNode::QueryNode(std::unique_ptr<SourceNode> source,
                     std::unique_ptr<JoinNode> join_clause,
                     std::unique_ptr<WhereNode> where_clause,
                     std::unique_ptr<SelectNode> select_clause,
                     size_t limit,
                     std::unique_ptr<OrderByNode> order_by,
                     std::unique_ptr<GroupByNode> group_by)
    : source_(std::move(source)), 
      join_clause_(std::move(join_clause)), 
      where_clause_(std::move(where_clause)), 
      select_clause_(std::move(select_clause)),
      limit_(limit),
      order_by_(std::move(order_by)),
      group_by_(std::move(group_by)) {}"""

repl_ast_cpp = """QueryNode::QueryNode(std::unique_ptr<SourceNode> source,
                     std::vector<std::unique_ptr<JoinNode>> join_clauses,
                     std::unique_ptr<WhereNode> where_clause,
                     std::unique_ptr<SelectNode> select_clause,
                     size_t limit,
                     std::unique_ptr<OrderByNode> order_by,
                     std::unique_ptr<GroupByNode> group_by)
    : source_(std::move(source)), 
      join_clauses_(std::move(join_clauses)), 
      where_clause_(std::move(where_clause)), 
      select_clause_(std::move(select_clause)),
      limit_(limit),
      order_by_(std::move(order_by)),
      group_by_(std::move(group_by)) {}"""

patch_file("src/ast/ast.cpp", [target_ast_cpp], [repl_ast_cpp])

# 3. Update src/parser/parser.cpp
target_parser_1 = """    std::unique_ptr<ast::JoinNode> join_clause = nullptr;"""
repl_parser_1 = """    std::vector<std::unique_ptr<ast::JoinNode>> join_clauses;"""

target_parser_2 = """        } else if (Match({lexer::TokenType::JOIN})) {
            if (join_clause) {
                throw errors::ParserError("Multiple JOIN clauses found", Previous().line, Previous().col);
            }
            join_clause = ParseJoin();"""
repl_parser_2 = """        } else if (Match({lexer::TokenType::JOIN})) {
            join_clauses.push_back(ParseJoin());
            while(Match({lexer::TokenType::JOIN})) {
                join_clauses.push_back(ParseJoin());
            }"""

target_parser_3 = """return std::make_unique<ast::QueryNode>(std::move(source), std::move(join_clause), std::move(where_clause), std::move(select_clause), limit, std::move(order_by), std::move(group_by));"""
repl_parser_3 = """return std::make_unique<ast::QueryNode>(std::move(source), std::move(join_clauses), std::move(where_clause), std::move(select_clause), limit, std::move(order_by), std::move(group_by));"""

patch_file("src/parser/parser.cpp", [target_parser_1, target_parser_2, target_parser_3], [repl_parser_1, repl_parser_2, repl_parser_3])

# 4. Update src/runtime/runtime.h
target_runtime_h = """void ExecuteStreaming(adapters::IAdapter& adapter,
                          const ast::SelectNode* select_node,
                          const ast::WhereNode* where_node, const ast::JoinNode* join_node, size_t limit);"""
repl_runtime_h = """void ExecuteStreaming(adapters::IAdapter& adapter,
                          const ast::SelectNode* select_node,
                          const ast::WhereNode* where_node, const std::vector<std::unique_ptr<ast::JoinNode>>& join_nodes, size_t limit);"""

patch_file("src/runtime/runtime.h", [target_runtime_h], [repl_runtime_h])

# 5. Update src/runtime/runtime.cpp
with open("src/runtime/runtime.cpp", "r") as f:
    r_content = f.read()

r_content = r_content.replace(
    "const ast::JoinNode* join_node = query->GetJoin();",
    "const std::vector<std::unique_ptr<ast::JoinNode>>& join_nodes = query->GetJoins();"
)
r_content = r_content.replace(
    "ExecuteStreaming(*adapter, select_node, where_node, join_node, limit);",
    "ExecuteStreaming(*adapter, select_node, where_node, join_nodes, limit);"
)

target_exec_stream = """void Runtime::ExecuteStreaming(adapters::IAdapter& adapter,
                               const ast::SelectNode* select_node,
                               const ast::WhereNode* where_node, const ast::JoinNode* join_node, size_t limit) {"""

if target_exec_stream not in r_content:
    print("Could not find ExecuteStreaming signature in runtime.cpp")
    sys.exit(1)

# Keep everything before ExecuteStreaming intact
head, tail = r_content.split(target_exec_stream, 1)

# Find the end of ExecuteStreaming method (this is hacky but assumes ExecuteWithOrderBy follows)
end_target = "void Runtime::ExecuteWithOrderBy"
body, remainder = tail.split(end_target, 1)

new_exec_stream = """void Runtime::ExecuteStreaming(adapters::IAdapter& adapter,
                               const ast::SelectNode* select_node,
                               const ast::WhereNode* where_node, const std::vector<std::unique_ptr<ast::JoinNode>>& join_nodes, size_t limit) {
    bool header_printed = false;
    size_t output_count = 0;
    std::vector<std::string> field_names;

    auto process_row = [&](adapters::Row& row) {
        if (limit > 0 && output_count >= limit) return;
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

    std::vector<std::unordered_multimap<std::string, adapters::Row>> join_hash_maps(join_nodes.size());
    std::vector<std::vector<adapters::Row>> joined_rows_fallbacks(join_nodes.size());
    std::vector<bool> is_simple_equi_joins(join_nodes.size(), false);
    std::vector<std::string> primary_key_names(join_nodes.size());
    std::vector<std::string> secondary_key_names(join_nodes.size());
    size_t estimated_memory = 0;

    for (size_t i = 0; i < join_nodes.size(); ++i) {
        const auto& join_node = join_nodes[i];
        auto joined_adapter = GetAdapterForSource(join_node->GetSource());
        joined_adapter->Open(join_node->GetSource());

        const ast::ExpressionNode* cond = join_node->GetCondition();
        std::string left_id, right_id;

        if (cond->GetToken().type == lexer::TokenType::EQUALS && cond->GetLeft()->IsLiteral() && cond->GetRight()->IsLiteral()) {
            if (cond->GetLeft()->GetToken().type == lexer::TokenType::IDENTIFIER && cond->GetRight()->GetToken().type == lexer::TokenType::IDENTIFIER) {
                is_simple_equi_joins[i] = true;
                left_id = cond->GetLeft()->GetToken().value;
                right_id = cond->GetRight()->GetToken().value;
            }
        }

        while (joined_adapter->HasNext()) {
            if (rows_processed_ >= security::Limits::Get().max_rows_processed) {
                throw errors::SecurityError("Maximum row limit exceeded.");
            }
            adapters::Row joined_row = joined_adapter->Next();
            rows_processed_++;
            if (rows_processed_ % 10000 == 0) CheckTimeout();

            if (is_simple_equi_joins[i]) {
                if (secondary_key_names[i].empty()) {
                    if (joined_row.find(left_id) != joined_row.end()) {
                        secondary_key_names[i] = left_id;
                        primary_key_names[i] = right_id;
                    } else if (joined_row.find(right_id) != joined_row.end()) {
                        secondary_key_names[i] = right_id;
                        primary_key_names[i] = left_id;
                    } else {
                        secondary_key_names[i] = left_id;
                        primary_key_names[i] = right_id;
                    }
                }
                std::string key_val;
                auto it = joined_row.find(secondary_key_names[i]);
                if (it != joined_row.end()) {
                    key_val = it->second;
                }
                
                size_t row_memory = key_val.capacity() + 64;
                for (const auto& kv : joined_row) {
                    row_memory += kv.first.capacity() + kv.second.capacity() + 64;
                }
                estimated_memory += row_memory;
                if (estimated_memory > security::Limits::Get().max_memory_usage) throw errors::SecurityError("Memory limit exceeded.");
                
                join_hash_maps[i].insert({key_val, std::move(joined_row)});
            } else {
                size_t row_memory = 64;
                for (const auto& kv : joined_row) {
                    row_memory += kv.first.capacity() + kv.second.capacity() + 64;
                }
                estimated_memory += row_memory;
                if (estimated_memory > security::Limits::Get().max_memory_usage) throw errors::SecurityError("Memory limit exceeded.");
                joined_rows_fallbacks[i].push_back(std::move(joined_row));
            }
        }
        joined_adapter->Close();
    }

    std::function<void(size_t, adapters::Row)> evaluate_joins = [&](size_t join_idx, adapters::Row current_row) {
        if (limit > 0 && output_count >= limit) return;
        if (join_idx == join_nodes.size()) {
            process_row(current_row);
            return;
        }
        
        const auto& join_node = join_nodes[join_idx];
        if (is_simple_equi_joins[join_idx]) {
            std::string lookup_val;
            auto it = current_row.find(primary_key_names[join_idx]);
            if (it != current_row.end()) {
                lookup_val = it->second;
            }

            auto range = join_hash_maps[join_idx].equal_range(lookup_val);
            for (auto hash_it = range.first; hash_it != range.second; ++hash_it) {
                adapters::Row next_row = current_row;
                for (const auto& kv : hash_it->second) {
                    next_row[kv.first] = kv.second;
                }
                if (EvaluateCondition(join_node->GetCondition(), next_row)) {
                    evaluate_joins(join_idx + 1, std::move(next_row));
                }
            }
        } else {
            for (const auto& joined_row : joined_rows_fallbacks[join_idx]) {
                adapters::Row next_row = current_row;
                for (const auto& kv : joined_row) {
                    next_row[kv.first] = kv.second;
                }
                if (EvaluateCondition(join_node->GetCondition(), next_row)) {
                    evaluate_joins(join_idx + 1, std::move(next_row));
                }
            }
        }
    };

    while (adapter.HasNext()) {
        if (rows_processed_ >= security::Limits::Get().max_rows_processed) throw errors::SecurityError("Row limit exceeded.");
        adapters::Row primary_row = adapter.Next();
        rows_processed_++;
        if (rows_processed_ % 10000 == 0) CheckTimeout();

        if (!join_nodes.empty()) {
            evaluate_joins(0, std::move(primary_row));
        } else {
            process_row(primary_row);
        }
        if (limit > 0 && output_count >= limit) break;
    }
    
    if (!header_printed && !select_node->IsWildcard()) {
        for (const auto& expr : select_node->GetFields()) field_names.push_back(FormatExpression(expr.get()));
        formatter_.PrintHeader(field_names, format_);
    }
}
"""

with open("src/runtime/runtime.cpp", "w") as f:
    f.write(head + new_exec_stream + end_target + remainder)
print("Patched src/runtime/runtime.cpp successfully")

# 6. Update tests/test_main.cpp to add a 3-table join test
with open("tests/test_main.cpp", "r") as f:
    test_content = f.read()

test_injection = """
    // Multiple Join Test
    std::string test_users = "id,name\\n1,Alice\\n2,Bob\\n3,Charlie\\n";
    std::string test_orders = "order_id,user_id,amount\\n101,1,50\\n102,1,150\\n103,2,200\\n";
    std::string test_items = "item_id,o_id,item_name\\n1001,101,Book\\n1002,102,Laptop\\n1003,103,Phone\\n";

    { std::ofstream out("users.csv"); out << test_users; }
    { std::ofstream out("orders.csv"); out << test_orders; }
    { std::ofstream out("items.csv"); out << test_items; }

    try {
        auto tokens = qle::lexer::Lexer("from users.csv JOIN orders.csv ON id == user_id JOIN items.csv ON order_id == o_id select name, amount, item_name").Tokenize();
        qle::parser::Parser p(tokens);
        auto ast = p.Parse();
        qle::runtime::Runtime r;
        r.Execute(ast.get());
        std::cout << "Multiple joins test passed.\\n";
    } catch (const std::exception& e) {
        std::cerr << "Multiple joins test failed: " << e.what() << "\\n";
    }
    
    return 0;
"""

test_content = test_content.replace("return 0;", test_injection)

with open("tests/test_main.cpp", "w") as f:
    f.write(test_content)
print("Patched tests/test_main.cpp successfully")
