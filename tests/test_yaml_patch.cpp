#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/runtime.h"
#include "errors/errors.h"
#include <iostream>
#include <cassert>

using namespace qle;

void TestYamlAdapter() {
    std::cout << "Running Yaml Adapter Tests..." << std::endl;
    lexer::Lexer lexer("from tests/test.yaml select id, name, age");
    auto tokens = lexer.Tokenize();
    parser::Parser parser(tokens);
    auto query = parser.Parse();
    
    runtime::Runtime runtime;
    // Redirect output? Actually we just run it and see if it crashes.
    // Or we could verify rows.
    runtime.Execute(query.get());
}
