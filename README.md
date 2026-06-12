# QLE (Query Language Everywhere)

A lightweight, interpreted query language for querying multiple data sources using a consistent syntax.

## Features

- **Blazing Fast**: Optimized C++17 runtime with zero external dependencies.
- **Interactive REPL**: Explore your data interactively via the built-in shell.
- **Universal Querying**: Use SQL-like syntax (`SELECT`, `WHERE`, `ORDER BY`, `LIMIT`, `GROUP BY`) on flat files.
- **Cross-Platform SQLite**: Natively query `.sqlite` files. The SQLite engine is bundled directly into the binary!
- **Data Transformations**: Perform inline string manipulation using `upper()`, `lower()`, `concat()`, etc.
- **Aggregations**: Bucket your data using `GROUP BY` and compute `sum`, `avg`, `min`, `max`.
- **Modular Adapters**: Currently supports `CSV`, `JSON`, and `SQLite`.
- **Typo Suggestions**: Helpful Levenshtein distance-based suggestions when you mistype a field name.

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
