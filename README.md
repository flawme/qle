# QLE (Query Language Everywhere)

A lightweight, interpreted query language for querying multiple data sources using a consistent syntax.

## Features

- **Zero-Dependency Core:** Written entirely in modern C++17 without heavy external data-processing libraries.
- **Interactive REPL**: Explore your data interactively via the built-in shell.
- **Multiple Data Formats:** Supports CSV, TSV, JSON, SQLite, YAML, and XML out of the box with zero-copy stream parsing.
- **Map-Reduce Parallel Execution:** Threaded `GROUP BY` logic and CSV adapter chunk splitting via `std::thread::hardware_concurrency()` for massive multicore throughput.
- **Incremental Aggregation:** `sum`, `count`, `min`, `max` process inline during streaming, completely eliminating row-caching memory footprints (10M row RAM usage is just 4.5 MB).
- **Projection Pushdown:** Columns not needed by the AST are skipped entirely at the parser level via read-masks, improving parse speeds dramatically (10M row queries in 3.7 seconds).
- **Modular Engine Architecture:** The monolithic `runtime.cpp` was cleanly decoupled into 5 focused component files (`evaluator.cpp`, `executor_streaming.cpp`, etc.).
- **Subqueries, CTEs & Complex Relations:** Supports recursive subquery evaluations, Common Table Expressions (`WITH`), and infinite chained `$O(1)` Hash Joins natively.
- **Data Transformations:** Perform inline string manipulation using `upper()`, `lower()`, `concat()`, `now()`, `year()`, etc.
- **Advanced Querying:** Supports nested `AND`/`OR` filtering, `GROUP BY` aggregations, `HAVING` filters, `ORDER BY` sorting, and mathematical/date inline functions.
- **`LIKE` Filtering:** Supports powerful wildcard regex pattern matching out of the box.
- **Execution Limits:** Strict memory-capping and row-processing timeouts protect the engine against recursive loops and adversarial file structures.

## Documentation

- [Usage Guide](docs/usage.md): Learn the QLE syntax, inline functions, and CLI limits.
- [Architecture](docs/architecture.md): Explore the engine's modular adapters and AST.
- [Performance Guide](docs/performance.md): Deep dive into the zero-copy engine, memory streaming, and big-data optimization strategies.
- **[Comprehensive Benchmarks](BENCHMARK.md)**: Hardware metrics and execution times across all 16 features.
- [Examples Library](docs/examples/README.md): Explore ready-to-run `.qle` files demonstrating joins, maths, and aggregations.

## Build

Requirements: CMake 3.14+, C++17 compiler.

```sh
mkdir build
cd build
cmake ..
make
```

## Usage

Run an inline query:
```sh
./qle "from users.csv where age > 18 select name, email"
```

Run queries from files:
```sh
./qle run query1.qle query2.qle
```

## Architecture

- src/lexer: Tokenization and position tracking.
- src/parser: Syntax validation and AST generation.
- src/ast: Immutable Abstract Syntax Tree nodes.
- src/runtime: Evaluates the AST against data sources.
- src/adapters: Extensible interfaces for CSV, JSON, etc.
