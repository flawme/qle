#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/runtime.h"
#include "errors/errors.h"
#include "logger/logger.h"
#include "utils/formatter.h"
#include "repl/repl.h"
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <vector>

using namespace qle;

static const char* VERSION_STRING = "qle v0.1.2";

static const char* HELP_TEXT =
    "Usage:\n"
    "  qle [options] \"<query>\"\n"
    "  qle [options] run <file1.qle> [file2.qle ...]\n"
    "\n"
    "Options:\n"
    "  --help              Show this help message and exit\n"
    "  --version           Print version information and exit\n"
    "  --format <mode>     Set output format: table, csv, json (default: csv)\n"
    "  --time              Show execution time on stderr\n"
    "  --quiet             Suppress non-essential output\n"
    "\n"
    "Examples:\n"
    "  qle \"from users.csv select *\"\n"
    "  qle --format table \"from users.csv where age > 18 select name, age\"\n"
    "  qle --time \"from data.csv select count\"\n"
    "  qle run query1.qle query2.qle\n";

std::string ReadFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filepath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

struct CliOptions {
    utils::OutputFormat format = utils::OutputFormat::CSV;
    bool show_time = false;
    bool quiet = false;
    std::vector<std::string> args;
};

CliOptions ParseFlags(int argc, char** argv) {
    CliOptions opts;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            std::cout << HELP_TEXT;
            std::exit(0);
        } else if (arg == "--version") {
            std::cout << "qle 0.1.3" << std::endl;
            std::exit(0);
        }
        if (arg == "--time") {
            opts.show_time = true;
            continue;
        }
        if (arg == "--quiet") {
            opts.quiet = true;
            continue;
        }
        if (arg == "--format") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --format requires an argument (table, csv, json)\n";
                std::exit(1);
            }
            std::string fmt = argv[++i];
            if (fmt == "table")     opts.format = utils::OutputFormat::TABLE;
            else if (fmt == "csv")  opts.format = utils::OutputFormat::CSV;
            else if (fmt == "json") opts.format = utils::OutputFormat::JSON;
            else {
                std::cerr << "Error: unknown format '" << fmt << "'. Use table, csv, or json.\n";
                std::exit(1);
            }
            continue;
        }

        opts.args.push_back(arg);
    }

    return opts;
}

void ExecuteQuery(const std::string& query, const CliOptions& opts) {
    auto start = std::chrono::high_resolution_clock::now();
    try {
        lexer::Lexer lexer(query);
        auto tokens = lexer.Tokenize();

        parser::Parser parser(tokens);
        auto ast = parser.Parse();

        runtime::Runtime runtime;
        runtime.SetFormat(opts.format);
        runtime.Execute(ast.get());
    } catch (const errors::QleException& e) {
        std::cerr << e.GetFormattedMessage() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Critical Error: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown Critical Error occurred." << std::endl;
    }
    
    if (opts.show_time) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cerr << "Execution time: " << duration.count() << " ms" << std::endl;
    }
}

int main(int argc, char** argv) {
    Debug::SetLogLevel(LogLevel::INFO);



    CliOptions opts = ParseFlags(argc, argv);

    if (opts.quiet) {
        Debug::Enable(false);
    }

    if (opts.args.empty()) {
        repl::Repl::Start(opts.format, opts.show_time);
        return 0;
    }

    if (opts.args[0] == "run" && opts.args.size() >= 2) {
        for (size_t i = 1; i < opts.args.size(); ++i) {
            const std::string& filepath = opts.args[i];
            if (!opts.quiet) {
                std::cout << "--- Executing " << filepath << " ---" << std::endl;
            }
            try {
                std::string query = ReadFile(filepath);
                ExecuteQuery(query, opts);
            } catch (const std::exception& e) {
                std::cerr << "Error reading file " << filepath << ": " << e.what() << std::endl;
            }
        }
    } else {
        ExecuteQuery(opts.args[0], opts);
    }

    return 0;
}
