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
    assert(tokens.size() == 9);
}

void TestParser() {
    std::cout << "Running Parser Tests..." << std::endl;
    lexer::Lexer lexer("from users where age > 18 select name");
    auto tokens = lexer.Tokenize();
    parser::Parser parser(tokens);
    auto ast = parser.Parse();
    assert(ast != nullptr);
}

void TestSecurity() {
    std::cout << "Running Security Tests..." << std::endl;
    std::string long_string = "from a where b == \"";
    long_string.append(9000, 'A'); 
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
    for(int i = 0; i < 150; ++i) query += "(";
    query += "a > b";
    for(int i = 0; i < 150; ++i) query += ")";
    query += " select a";
    bool caught = false;
    try {
        lexer::Lexer lexer(query);
        parser::Parser parser(lexer.Tokenize());
        parser.Parse();
    } catch (const errors::SecurityError&) { caught = true; }
    assert(caught);
}

void TestDynamicLimits() {
    std::cout << "Running Dynamic Limits Tests..." << std::endl;
    size_t old_limit = security::Limits::Get().max_ast_nodes;
    security::Limits::Get().max_ast_nodes = 3;
    bool caught = false;
    try {
        lexer::Lexer lexer("from users where a > b select c, d");
        parser::Parser parser(lexer.Tokenize());
        parser.Parse();
    } catch (const errors::SecurityError&) { caught = true; }
    security::Limits::Get().max_ast_nodes = old_limit;
    assert(caught);
}

void TestAdversarialParsing() {
    std::cout << "Running Adversarial Parsing Tests..." << std::endl;
    std::string queries[] = {
        "from users order by",
        "from users select",
        "from users where",
        "from users limit",
        "from users limit abc",
        "from users limit 0",
        "from users limit 9999999999999999999999999999999999999999999"
    };
    for (const auto& q : queries) {
        try {
            lexer::Lexer lexer(q);
            parser::Parser parser(lexer.Tokenize());
            parser.Parse();
            assert(false && "Should have thrown for invalid query");
        } catch (const errors::QleException&) {}
    }
}

void TestMalformedDataSources() {
    std::cout << "Running Malformed Data Sources Tests..." << std::endl;
    try {
        runtime::Runtime rt;
        lexer::Lexer lexer("from tests/malformed.json select *");
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
    } catch (const errors::QleException&) {}

    try {
        runtime::Runtime rt;
        lexer::Lexer lexer("from tests/empty.json select *");
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
    } catch (const errors::QleException&) {}
}

void TestBoundaryConditions() {
    std::cout << "Running Boundary Conditions Tests..." << std::endl;
    std::string query = "from tests/malformed.csv where name > 5 select id";
    try {
        runtime::Runtime rt;
        lexer::Lexer lexer(query);
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
    } catch (const errors::QleException&) {}
    
    try {
        lexer::Lexer lexer("from \xff select *");
        lexer.Tokenize();
    } catch (const errors::QleException&) {}
}

int main() {
    Debug::Enable(false); 
    TestLexer();
    TestParser();
    TestSecurity();
    TestRecursionDepth();
    TestDynamicLimits();
    TestAdversarialParsing();
    TestMalformedDataSources();
    TestBoundaryConditions();
    std::cout << "All extreme tests passed!" << std::endl;
    return 0;
}
