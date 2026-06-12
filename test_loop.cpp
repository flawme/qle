#include "lexer/lexer.h"
#include "parser/parser.h"
#include "errors/errors.h"
#include <iostream>

using namespace qle;

int main() {
    std::string queries[] = {
        "from users order by",
        "from users select",
        "select name from users",
        "from users where",
        "from users limit",
        "from users limit abc",
        "from users limit 0",
        "from users limit 9999999999999999999999999999999999999999999",
    };

    for (const auto& q : queries) {
        std::cout << "Testing: " << q << std::endl;
        try {
            lexer::Lexer lexer(q);
            parser::Parser parser(lexer.Tokenize());
            parser.Parse();
            std::cout << "Did not throw for: " << q << std::endl;
        } catch (const errors::QleException& e) {
            std::cout << "Caught QleException: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Caught std::exception: " << e.what() << std::endl;
        }
    }
    return 0;
}
