#include "repl/repl.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/runtime.h"
#include "errors/errors.h"
#include <iostream>
#include <string>
#include <chrono>

namespace qle {
namespace repl {

void Repl::Start(utils::OutputFormat format, bool show_time) {
    std::cout << "QLE Interactive Shell\nType 'exit' or 'quit' to quit.\n";
    
    std::string line;
    while (true) {
        std::cout << "qle > ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        // Trim whitespace
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue; // Empty line
        size_t last = line.find_last_not_of(" \t\r\n");
        std::string query = line.substr(first, (last - first + 1));

        if (query == "exit" || query == "quit") {
            break;
        }

        auto start = std::chrono::high_resolution_clock::now();
        try {
            lexer::Lexer lexer(query);
            auto tokens = lexer.Tokenize();

            parser::Parser parser(tokens);
            auto ast = parser.Parse();

            runtime::Runtime runtime;
            runtime.SetFormat(format);
            runtime.Execute(ast.get());
        } catch (const errors::QleException& e) {
            std::cerr << e.GetFormattedMessage() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Critical Error: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown Critical Error occurred." << std::endl;
        }

        if (show_time) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cerr << "Execution time: " << duration.count() << " ms" << std::endl;
        }
    }
}

} // namespace repl
} // namespace qle
