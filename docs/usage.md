# Usage Guide

QLE uses a simplified SQL-like syntax targeted specifically at flat data files.

## Basic Syntax

A valid QLE query requires a `from` and a `select` clause. The `where`, `order by`, and `limit` clauses are optional.

```text
from <source_file>
[where <condition>]
select <field1, field2 | * | count>
[order by <field> [asc|desc]]
[limit <number>]
```

### Select Options
- **Specific fields:** `select name, email`
- **All fields:** `select *`
- **Count matching rows:** `select count`

### Supported Operators
- Equality: `==`, `!=`
- Comparison: `>`, `<`, `>=`, `<=`
- Logical: `and`, `or`
- Grouping: `(` and `)`

## CLI Options

The CLI supports several options to customize execution:

```bash
qle [options] "<query>"
qle [options] run <file1.qle> [file2.qle ...]

Options:
  --help              Show help message
  --version           Show version information
  --format <mode>     Set output format: table, csv, json (default: csv)
  --time              Show execution time in milliseconds
  --quiet             Suppress info messages (like file execution banners)
```

### Examples

**Run an inline query with table output and timing:**
```bash
./qle --format table --time "from users.csv where age > 18 order by age desc limit 5 select name, age"
```

**Count matching rows:**
```bash
./qle "from data.json where status == 'active' select count"
```

## Running File-Based Queries

For complex queries that require multiple lines, save your query to a `.qle` file:

**`my_query.qle`**:
```text
from users.json
where (age >= 21 or role == "admin")
  and active == 1
select name, role, email
order by name asc
limit 10
```

Execute the file using the `run` command:

```bash
./qle run my_query.qle
```

You can execute multiple query files sequentially:

```bash
./qle run query1.qle query2.qle
```

## Typo Suggestions
If you misspell a field name in your query, QLE will automatically suggest the closest matching field using Levenshtein distance:
```text
Error: Unknown field: agge (Did you mean 'age'?)
```
