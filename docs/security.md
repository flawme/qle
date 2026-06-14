# Security & Limits

QLE prioritizes memory safety and predictable resource usage. Due to the dangers of interpreting dynamic user queries, the parser and runtime are heavily sandboxed via configurable limits.

## Memory Caps (`src/security/limits.h`)

- **Incremental Aggregation & Projection Pushdown**: While not explicit strict caps, QLE inherently prevents memory exhaustion by processing aggregations (`sum`, `count`) inline without caching rows. Furthermore, unneeded columns are skipped entirely at the parser level via read-masks. This allows processing massive datasets securely; for example, a 10M row file only requires ~4.5 MB of active RAM.


QLE dynamically enforces the following caps at runtime:

- **Maximum File Size (100 MB)**: The Adapters will refuse to process source files exceeding this limit.
- **Maximum Query Size (64 KB)**: The Lexer will reject massive strings immediately upon instantiation.
- **Maximum Tokens (100,000)**: Prevents denial-of-service via massive repetition (e.g., repeating the `AND` keyword).
- **Maximum AST Nodes (50,000)**: Rejects excessively complex logic trees.
- **Maximum Rows Processed (1,000,000)**: Protects the system from unbounded loops during processing.
- **Maximum String Length (8,192 characters)**: Protects against runaway string allocations.
- **Maximum Recursion Depth (128)**: A `RecursionGuard` inside the Parser prevents C++ stack overflow vulnerabilities caused by heavily nested parentheses and infinite nested Subqueries.
- **Strict Hash-Join Caps**: The `std::unordered_multimap` used for `JOIN` logic precisely estimates string capacities and violently throws memory cap exceptions if overloaded.
- **LIKE Execution Limit**: The regex-style wildcard engine uses a strict `10,000` execution step timeout to completely protect against ReDoS (Regular Expression Denial of Service) hangs.
- **Time/Epoch UB Overflow Bounds**: The `year()`, `month()`, and `day()` parsers securely catch malformed `YYYY` string inputs to prevent `<chrono>` epoch math integer overflows.

## Exception Safety

Uncaught exceptions are strictly forbidden. All logic exceptions derive from `qle::errors::QleException`. 
The CLI wrapper catches these errors and safely formats them into user-friendly responses. **Internal stack traces or C++ segfaults are never exposed to the end-user.**

## Path Traversal Protection
A centralized path validator (`src/security/path_validator.cpp`) blocks `..` path components to prevent directory traversal attacks. Subdirectory and absolute paths are allowed, so queries like `from data/users.csv` or `from /home/user/data.csv` work normally.
