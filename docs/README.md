# QLE Documentation

Welcome to the documentation for **QLE (Query Language Everywhere)**. 

QLE is a lightweight, interpreted query language designed to allow developers to query multiple data sources (like CSV and JSON) using a unified, strict syntax, without generating complex machine code.


## Core Features & Updates

- **Map-Reduce Parallel Execution:** Threaded `GROUP BY` logic and CSV adapter chunk splitting via `std::thread::hardware_concurrency()` for massive multicore throughput.
- **Incremental Aggregation:** `sum`, `count`, `min`, `max` process inline during streaming, completely eliminating row-caching memory footprints (10M row RAM usage is just 4.5 MB).
- **Projection Pushdown:** Columns not needed by the AST are skipped entirely at the parser level via read-masks, improving parse speeds dramatically (10M row queries in 3.7 seconds).
- **Modular Engine Architecture:** The monolithic `runtime.cpp` was decoupled into 5 separate component files (`evaluator.cpp`, `executor_streaming.cpp`, etc.).

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
