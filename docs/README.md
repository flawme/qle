# QLE Documentation

Welcome to the documentation for **QLE (Query Language Everywhere)**. 

QLE is a lightweight, interpreted query language designed to allow developers to query multiple data sources (like CSV and JSON) using a unified, strict syntax, without generating complex machine code.

## Table of Contents

1. [Architecture Overview](architecture.md)
   - Understand how the Lexer, Parser, AST, and Runtime modules interact.
2. [Security & Limits](security.md)
   - Learn about the explicit memory caps and protections against resource exhaustion.
3. [Usage Guide](usage.md)
   - Learn how to write QLE queries and execute them from the CLI.
4. [Development & Adapters](development.md)
   - Instructions on building the project and extending it with new data source adapters.
5. [Performance & Optimization](performance.md)
   - Deep dive into the zero-copy parsing engine, memory streaming, and linear mapping optimizations.
