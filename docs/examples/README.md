# QLE Examples Library

This directory contains standalone `.qle` files demonstrating how to use the various capabilities of the QLE engine.

To run any of these examples, use the `run` command from the root of the project:

```bash
./qle run docs/examples/join.qle
```

## Available Examples

- `join.qle`: Demonstrates how to use the `$O(1)` Hash Join engine to merge two different data sources based on a matching key.
- `maths.qle`: Demonstrates how to use inline mathematical expressions like `abs()` and `round()`.
- `functions.qle`: Demonstrates how to format strings dynamically using `upper()`, `lower()`, `concat()`, and `length()`.
- `aggregations.qle`: Demonstrates how to bucket flat data into groupings and run mathematical aggregations like `sum()`, `avg()`, `min()`, and `max()`.
- `subqueries.qle`: Demonstrates how to use nested recursive engine evaluations directly inside the `FROM` clause.
- `multiple_joins.qle`: Demonstrates infinite `$O(1)` Hash Join chaining across multiple tables.
- `xml_parsing.qle`: Demonstrates the zero-dependency XML recursive streaming parser and the `LIKE` wildcard operator.
