# Architecture Overview

QLE is built using a classic interpreter pipeline with strict boundaries and single-responsibility modules. The pipeline avoids JIT compilation or machine-code generation in favor of a lightweight Abstract Syntax Tree (AST) walker.

## 1. Lexer (`src/lexer`)
The Lexer's sole responsibility is tokenization. It streams through the raw query string character-by-character and converts it into a sequence of `Token` objects (e.g., `IDENTIFIER`, `NUMBER`, `STRING`, `WHERE`, `>`).
- It enforces string limits.
- It enforces a maximum token limit to prevent memory flooding.

## 2. Parser (`src/parser`)
The Parser takes the token stream and constructs an Abstract Syntax Tree (AST). It performs syntax validation without executing any logic.
- It uses a Recursive Descent approach for expressions.
- Includes a `RecursionGuard` to prevent stack overflows (limits depth to 128).
- Tracks AST node allocations to prevent AST explosion.

## 3. Abstract Syntax Tree (`src/ast`)
The AST nodes are lightweight and strictly immutable after construction.
- `QueryNode`: The root node containing the data source, condition, and selected fields. It optionally holds a `JoinNode`.
- `JoinNode`: Defines a secondary cross-file data source and the boolean ON condition expression.
- `ExpressionNode`: Represents binary operations or literal values.

## 4. Runtime (`src/runtime`)
The Runtime executes the AST. It handles filtering, evaluating inline functions (`upper`, `abs`, etc.), grouping data via the `group by` clause, sorting rows based on `OrderByNode`, and performing multi-file hash joins.

## 5. Adapters (`src/adapters`)
Data extraction is entirely decoupled from the runtime. Adapters implement the `IAdapter` interface, guaranteeing `Open()`, `HasNext()`, `Next()`, and `Close()` functionality. Supported formats are:
- **CSV**: Streams comma-separated text.
- **JSON**: Streams JSON object arrays.
- **SQLite**: Connects natively to `.sqlite` or `.db` databases using the SQLite C API. 
- **YAML**: Recursively streams YAML flat objects dynamically.

## 6. Utils & Tools (`src/utils`, `src/repl`)
- **Interactive REPL:** Provides a live shell loop (`src/repl/repl.cpp`) for ad-hoc querying.
- **Formatter:** Handles turning the raw row data into `table`, `json`, or `csv` outputs.
- **Suggestions:** Implements the Levenshtein distance algorithm to provide "Did you mean?" typo corrections for unknown fields.
