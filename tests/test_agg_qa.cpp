#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/runtime.h"
#include "errors/errors.h"
#include <iostream>
#include <cassert>
#include <fstream>

using namespace qle;

void test1() {
    try {
        lexer::Lexer lexer("from \"");
        lexer.Tokenize();
    } catch (const errors::LexerError&) {}
}

int main() {
    test1();
    std::cout << "All QA tests pass." << std::endl;
    return 0;
}
