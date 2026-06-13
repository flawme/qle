#include "runtime/runtime.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include <iostream>
#include <fstream>

using namespace qle;

int main() {
    std::ofstream o1("t1.csv");
    o1 << "id,name\n1,alice\n2,bob\n";
    o1.close();
    
    std::ofstream o2("t2.csv");
    o2 << "id2,age\n1,30\n2,40\n";
    o2.close();

    lexer::Lexer lexer("from t1.csv join t2.csv on id == id2 select id, name, age");
    parser::Parser parser(lexer.Tokenize());
    runtime::Runtime rt;
    rt.Execute(parser.Parse().get());
    return 0;
}
