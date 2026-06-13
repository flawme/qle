#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/runtime.h"
#include "security/limits.h"
#include "errors/errors.h"
#include <iostream>
#include <fstream>
#include <cassert>

using namespace qle;

int main() {
    std::ofstream right_table("right_huge.csv");
    right_table << "id,value\n";
    std::string large_val(10000, 'A');
    for (int i = 0; i < 50000; i++) {
        right_table << i << "," << large_val << "\n";
    }
    right_table.close();

    std::ofstream left_table("left_small.csv");
    left_table << "id\n1\n";
    left_table.close();

    size_t old_limit = security::Limits::Get().max_rows_processed;
    security::Limits::Get().max_rows_processed = 1000000; // Large row limit to allow the memory issue to trigger
    
    try {
        std::string query = "from left_small.csv join right_huge.csv on id == id select id, value";
        qle::lexer::Lexer lexer(query);
        qle::parser::Parser parser(lexer.Tokenize());
        qle::runtime::Runtime rt;
        rt.Execute(parser.Parse().get());
        std::cerr << "FAIL: Did not catch memory overflow!\n";
        return 1;
    } catch (const errors::SecurityError& e) {
        std::cout << "Caught: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Other error: " << e.what() << "\n";
        return 1;
    }
    security::Limits::Get().max_rows_processed = old_limit;
    return 0;
}
