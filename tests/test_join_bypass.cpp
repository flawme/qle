#include "runtime/runtime.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "security/limits.h"
#include "errors/errors.h"
#include <iostream>
#include <fstream>
#include <cassert>

using namespace qle;

int main() {
    std::ofstream o1("t1.csv");
    o1 << "id\n1\n2\n";
    o1.close();
    
    std::ofstream o2("t2.csv");
    o2 << "id\n";
    for(int i=0; i<100000; i++) o2 << i << "\n";
    o2.close();

    size_t old_limit = security::Limits::Get().max_rows_processed;
    security::Limits::Get().max_rows_processed = 100; // very low

    bool caught = false;
    try {
        lexer::Lexer lexer("from t1.csv join t2.csv on id == id select id");
        parser::Parser parser(lexer.Tokenize());
        runtime::Runtime rt;
        rt.Execute(parser.Parse().get());
    } catch (const errors::SecurityError& e) {
        caught = true;
    }
    
    security::Limits::Get().max_rows_processed = old_limit;
    if (!caught) {
        std::cerr << "VULNERABILITY: JOIN bypasses row limits!" << std::endl;
        return 1;
    }
    return 0;
}
