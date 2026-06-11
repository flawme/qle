# Usage Guide

QLE uses a simplified SQL-like syntax targeted specifically at flat data files.

## Basic Syntax

A valid QLE query requires a `from` and a `select` clause. The `where` clause is optional.

```text
from <source_file>
where <condition>
select <field1>, <field2>
```

### Supported Operators
- Equality: `==`, `!=`
- Comparison: `>`, `<`, `>=`, `<=`
- Logical: `and`, `or`
- Grouping: `(` and `)`

## Running Inline Queries

Pass the query string directly to the QLE executable:

```bash
./qle "from users.csv where age > 18 and score > 50 select first_name, email"
```

## Running File-Based Queries

For complex queries that require multiple lines, save your query to a `.qle` file:

**`my_query.qle`**:
```text
from users.json
where (age >= 21 or role == "admin")
  and active == 1
select name, role, email
```

Execute the file using the `run` command:

```bash
./qle run my_query.qle
```

You can execute multiple query files sequentially:

```bash
./qle run query1.qle query2.qle
```
