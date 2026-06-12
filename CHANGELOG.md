# QLE Changelog

## [v0.1.2] - 2026-06-12

### Fixed
- **Integer Overflow**: `limit` clauses containing values that exceed maximum limits now correctly throw an `errors::ParserError` instead of allowing a C++ `std::out_of_range` unhandled exception to crash the process.
- **Undefined Behavior in Lexer**: Functions operating on individual characters (like `std::isalpha` or `std::isdigit`) now safely cast characters to `unsigned char`. This prevents potential segmentation faults triggered by negative byte values (like `\xff`) in malformed or malicious queries.
- **Undefined Behavior in JSON Adapter**: Similarly, fixed a `ctype` bounds violation inside the JSON parsing logic, ensuring resilience against maliciously encoded JSON sources.
- **Cleaned Up Test Suite**: The aggressive QA tests have been fully integrated into the `tests/` directory and connected to the standard `ctest` command runner, preventing raw compiled binaries from leaking into version control.
