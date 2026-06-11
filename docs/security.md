# Security & Limits

QLE prioritizes memory safety and predictable resource usage. Due to the dangers of interpreting dynamic user queries, the parser and runtime are heavily sandboxed via configurable limits.

## Memory Caps (`src/security/limits.h`)

QLE dynamically enforces the following caps at runtime:

- **Maximum File Size (100 MB)**: The Adapters will refuse to process source files exceeding this limit.
- **Maximum Query Size (64 KB)**: The Lexer will reject massive strings immediately upon instantiation.
- **Maximum Tokens (100,000)**: Prevents denial-of-service via massive repetition (e.g., repeating the `AND` keyword).
- **Maximum AST Nodes (50,000)**: Rejects excessively complex logic trees.
- **Maximum Rows Processed (1,000,000)**: Protects the system from unbounded loops during processing.
- **Maximum String Length (8,192 characters)**: Protects against runaway string allocations.
- **Maximum Recursion Depth (128)**: A `RecursionGuard` inside the Parser prevents C++ stack overflow vulnerabilities caused by heavily nested parentheses.

## Exception Safety

Uncaught exceptions are strictly forbidden. All logic exceptions derive from `qle::errors::QleException`. 
The CLI wrapper catches these errors and safely formats them into user-friendly responses. **Internal stack traces or C++ segfaults are never exposed to the end-user.**

## Path Traversal Protection
Adapters inspect source paths to explicitly block inputs containing `..` or `/` patterns, isolating execution strictly to the expected directory.
