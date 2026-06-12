# QLE Changelog

## [v0.1.3] - 2026-06-12

### Added
- **Interactive REPL Shell:** Launch QLE without arguments to enter an interactive query shell.
- **SQLite Support:** A native SQLite adapter allows querying `.sqlite` and `.db` files directly. The SQLite amalgamation (v3.53) is bundled directly into the engine, making QLE a zero-dependency cross-platform utility!
- **Aggregations & Grouping:** Added support for `group by` alongside aggregation functions like `sum`, `avg`, `min`, `max`, and `count`.
- **Inline Functions:** Added dynamic string manipulation functions `upper()`, `lower()`, `concat()`, and `length()`.

### Fixed
- **Undefined Behavior:** Fixed an issue where `upper()` and `lower()` could trigger segmentation faults on negative `char` values (like UTF-8 bytes) by safely wrapping them with `unsigned char`.
## [v0.1.2] - 2026-06-12

### Fixed
- **Integer Overflow**: `limit` clauses containing values that exceed maximum limits now correctly throw an `errors::ParserError` instead of allowing a C++ `std::out_of_range` unhandled exception to crash the process.
- **Undefined Behavior in Lexer**: Functions operating on individual characters (like `std::isalpha` or `std::isdigit`) now safely cast characters to `unsigned char`. This prevents potential segmentation faults triggered by negative byte values (like `\xff`) in malformed or malicious queries.
- **Undefined Behavior in JSON Adapter**: Similarly, fixed a `ctype` bounds violation inside the JSON parsing logic, ensuring resilience against maliciously encoded JSON sources.
- **Cleaned Up Test Suite**: The aggressive QA tests have been fully integrated into the `tests/` directory and connected to the standard `ctest` command runner, preventing raw compiled binaries from leaking into version control.
