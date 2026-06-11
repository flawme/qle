#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/runtime.h"
#include "errors/errors.h"
#include "logger/logger.h"
#include <iostream>
#include <string>

#include <fstream>
#include <sstream>

using namespace qle;

std::string ReadFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filepath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void ExecuteQuery(const std::string& query) {
    try {
        lexer::Lexer lexer(query);
        auto tokens = lexer.Tokenize();

        parser::Parser parser(tokens);
        auto ast = parser.Parse();

        runtime::Runtime runtime;
        runtime.Execute(ast.get());
    } catch (const errors::QleException& e) {
        std::cerr << e.GetFormattedMessage() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Critical Error: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown Critical Error occurred." << std::endl;
    }
}

int main(int argc, char** argv) {
    Debug::SetLogLevel(LogLevel::INFO);

    if (argc < 2) {
        std::cerr << "Usage: \n  qle \"<query>\"\n  qle run <file1.qle> [file2.qle ...]" << std::endl;
        return 1;
    }

    std::string arg1 = argv[1];

    if (arg1 == "run" && argc >= 3) {
        for (int i = 2; i < argc; ++i) {
            std::string filepath = argv[i];
            std::cout << "--- Executing " << filepath << " ---" << std::endl;
            try {
                std::string query = ReadFile(filepath);
                ExecuteQuery(query);
            } catch (const std::exception& e) {
                std::cerr << "Error reading file " << filepath << ": " << e.what() << std::endl;
            }
        }
    } else {
        ExecuteQuery(arg1);
    }

    return 0;
}
