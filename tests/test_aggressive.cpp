#include "runtime/runtime.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "errors/errors.h"
#include <iostream>

using namespace qle;

int main() {
    try {
        runtime::Runtime rt;
        lexer::Lexer lexer("from test.csv select upper(\"\xFF\xFF\")");
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
    } catch (...) {
    }
    std::cout << "Done" << std::endl;
    return 0;
}
