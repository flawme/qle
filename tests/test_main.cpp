#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/runtime.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "logger/logger.h"
#include <iostream>
#include <cassert>

using namespace qle;

void TestLexer() {
    std::cout << "Running Lexer Tests..." << std::endl;
    lexer::Lexer lexer("from users where age > 18 select name");
    auto tokens = lexer.Tokenize();
    // 8 tokens + 1 EOF = 9 tokens
    assert(tokens.size() == 9);
    assert(tokens[0].type == lexer::TokenType::FROM);
    assert(tokens[1].type == lexer::TokenType::IDENTIFIER);
    assert(tokens[2].type == lexer::TokenType::WHERE);
}

void TestParser() {
    std::cout << "Running Parser Tests..." << std::endl;
    lexer::Lexer lexer("from users where age > 18 select name");
    auto tokens = lexer.Tokenize();
    parser::Parser parser(tokens);
    auto ast = parser.Parse();
    assert(ast != nullptr);
    assert(ast->GetType() == ast::NodeType::QUERY);
    assert(ast->GetSource()->GetSourceName() == "users");
}

void TestSecurity() {
    std::cout << "Running Security Tests..." << std::endl;
    
    // Test extremely long string
    std::string long_string = "from a where b == \"";
    long_string.append(9000, 'A'); // limit is 8192
    long_string += "\" select c";
    
    bool caught = false;
    try {
        lexer::Lexer lexer(long_string);
        lexer.Tokenize();
    } catch (const errors::SecurityError&) {
        caught = true;
    }
    assert(caught);
}

void TestRecursionDepth() {
    std::cout << "Running Recursion Depth Tests..." << std::endl;
    std::string query = "from users where ";
    for(int i = 0; i < 150; ++i) { // Limit is 128
        query += "(";
    }
    query += "a > b";
    for(int i = 0; i < 150; ++i) {
        query += ")";
    }
    query += " select a";
    
    bool caught = false;
    try {
        lexer::Lexer lexer(query);
        auto tokens = lexer.Tokenize();
        parser::Parser parser(tokens);
        parser.Parse();
    } catch (const errors::SecurityError&) {
        caught = true;
    }
    assert(caught);
}

void TestDynamicLimits() {
    std::cout << "Running Dynamic Limits Tests..." << std::endl;
    // Lower the max AST nodes
    size_t old_limit = security::Limits::Get().max_ast_nodes;
    security::Limits::Get().max_ast_nodes = 3;
    
    bool caught = false;
    try {
        lexer::Lexer lexer("from users where a > b select c, d");
        auto tokens = lexer.Tokenize();
        parser::Parser parser(tokens);
        parser.Parse();
    } catch (const errors::SecurityError&) {
        caught = true;
    }
    
    // Restore limit
    security::Limits::Get().max_ast_nodes = old_limit;
    assert(caught);
}

int main() {
    Debug::Enable(false); // Disable logs for clean output
    TestLexer();
    TestParser();
    TestSecurity();
    TestRecursionDepth();
    TestDynamicLimits();
    
    std::cout << "All extreme tests passed!" << std::endl;
    return 0;
}
