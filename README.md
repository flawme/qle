# QLE (Query Language Everywhere)

A lightweight, interpreted query language for querying multiple data sources using a consistent syntax.

## Features

- Parse and execute queries directly without generating machine code.
- Strict memory-safe design patterns and explicit limits (recursion depth, file size, token limits).
- Pluggable adapter architecture. Currently supports CSV and JSON.
- Execute queries directly from the CLI or via .qle files.

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
