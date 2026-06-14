#include "runtime/vector_evaluator.h"
#include "ast/ast.h"
#include "lexer/token.h"
#include <cassert>
#include <iostream>

using namespace qle;

int main() {
    runtime::VectorBatch batch;
    batch.num_rows = 10;
    
    std::vector<double> col_a(10);
    for (int i = 0; i < 10; ++i) col_a[i] = i * 10.0;
    batch.numeric_columns["A"] = col_a;

    // A > 50
    lexer::Token t_a { lexer::TokenType::IDENTIFIER, "A", 0, 0 };
    lexer::Token t_50 { lexer::TokenType::NUMBER, "50", 0, 0 };
    lexer::Token t_gt { lexer::TokenType::GREATER_THAN, ">", 0, 0 };

    auto expr_a = std::make_unique<ast::ExpressionNode>(t_a);
    auto expr_50 = std::make_unique<ast::ExpressionNode>(t_50);
    ast::ExpressionNode cond(t_gt, std::move(expr_a), std::move(expr_50));

    std::vector<bool> result = runtime::EvaluateConditionVectorized(&cond, batch);
    
    assert(result.size() == 10);
    for (int i = 0; i < 10; ++i) {
        if (i > 5) assert(result[i] == true);
        else assert(result[i] == false);
    }
    std::cout << "Vectorized evaluation test passed." << std::endl;
    return 0;
}
