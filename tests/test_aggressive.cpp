#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/runtime.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "logger/logger.h"
#include <iostream>
#include <cassert>
#include <fstream>

using namespace qle;

void TestMalformedInputs() {
    std::cout << "Running Malformed Inputs Tests..." << std::endl;
    // Malformed JSON (missing closing brace)
    try {
        runtime::Runtime rt;
        lexer::Lexer lexer("from tests/malformed.json select *");
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
        assert(false && "Should have thrown on malformed json");
    } catch (const errors::QleException&) {}

    // Malformed JSON (empty)
    try {
        runtime::Runtime rt;
        lexer::Lexer lexer("from tests/empty.json select *");
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
        assert(false && "Should have thrown on empty json");
    } catch (const errors::QleException&) {}
}

void TestSyntaxEdgeCases() {
    std::cout << "Running Syntax Edge Cases..." << std::endl;
    std::string queries[] = {
        "from users order by",
        "from users select",
        "select name from users",
        "from users where",
        "from users limit",
        "from users limit abc",
        "from users limit 0",
        "from users limit 9999999999999999999999999999999999999999999", // should not crash but throw
    };

    for (const auto& q : queries) {
        try {
            lexer::Lexer lexer(q);
            parser::Parser parser(lexer.Tokenize());
            parser.Parse();
            assert(false && "Should have thrown");
        } catch (const errors::QleException&) {}
    }
}

void TestValueCoercion() {
    std::cout << "Running Value Coercion..." << std::endl;
    std::string query = "from tests/malformed.csv where name > 5 select id";
    try {
        runtime::Runtime rt;
        lexer::Lexer lexer(query);
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
        // Should handle it gracefully
    } catch (const errors::QleException&) {}
}

int main() {
    Debug::Enable(false);
    TestMalformedInputs();
    TestSyntaxEdgeCases();
    TestValueCoercion();
    std::cout << "All aggressive tests passed!" << std::endl;
    return 0;
}
