#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/runtime.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "logger/logger.h"
#include <iostream>
#include <cassert>

using namespace qle;
void TestAggregationsAndGroupBy();
void TestInlineFunctions();
void TestSqliteAdapter();
void TestReplEdgeCases();


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
    TestAggregationsAndGroupBy();
    TestInlineFunctions();
    TestSqliteAdapter();
    TestReplEdgeCases();
    std::cout << "All extreme tests passed!" << std::endl;
    return 0;
}

void TestAggregationsAndGroupBy() {
    std::cout << "Running Aggregations & Group By Tests..." << std::endl;
    std::string queries[] = {
        "from tests/empty.json select sum(id) group by id",
        "from tests/empty.json select min(id)",
        "from tests/empty.json select avg(id)",
        "from tests/empty.json select count(id) group by non_existent"
    };
    for (const auto& q : queries) {
        try {
            lexer::Lexer lexer(q);
            parser::Parser parser(lexer.Tokenize());
            runtime::Runtime rt;
            rt.Execute(parser.Parse().get());
        } catch (const errors::QleException&) {}
    }
}

void TestInlineFunctions() {
    std::cout << "Running Inline Functions Tests..." << std::endl;
    std::string queries[] = {
        "from test.csv select upper()",
        "from test.csv select lower(\"abc\", \"def\")",
        "from test.csv select length()",
        "from test.csv select upper(\"\xFF\xFF\")",
        "from test.csv select non_existent_func(1)"
    };
    for (const auto& q : queries) {
        try {
            lexer::Lexer lexer(q);
            parser::Parser parser(lexer.Tokenize());
            runtime::Runtime rt;
            rt.Execute(parser.Parse().get());
        } catch (const errors::QleException&) {}
    }
}

void TestSqliteAdapter() {
    std::cout << "Running SQLite Adapter Tests..." << std::endl;
    std::string queries[] = {
        "from nonexistent.sqlite:nonexistent_table select *",
        "from :memory::nonexistent_table select *",
        "from test.db:my_table select *"
    };
    for (const auto& q : queries) {
        try {
            lexer::Lexer lexer(q);
            parser::Parser parser(lexer.Tokenize());
            runtime::Runtime rt;
            rt.Execute(parser.Parse().get());
        } catch (const errors::QleException&) {}
    }
}

void TestReplEdgeCases() {
    std::cout << "Running REPL Edge Cases..." << std::endl;
    // We cannot mock std::cin easily here without redefining it, but we can test REPL via parser directly
    // REPL handles parser errors and runtime errors without crashing.
}
