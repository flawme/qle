# Usage Guide

QLE uses a simplified SQL-like syntax targeted specifically at flat data files.



## Interactive REPL Shell
To start an interactive session, simply run QLE without any arguments:

```bash
./qle
```
You will be dropped into a live `qle > ` prompt where you can execute queries and explore data interactively. Type `exit` or `quit` to leave the shell.

## Basic Syntax

A valid QLE query requires a `from` and a `select` clause. The `where`, `group by`, `order by`, and `limit` clauses are optional.

```text
from <source_file>
[join <source_file2> on <condition>]
[where <condition>]
[group by <field>]
select <field1, field2 | * | count | sum(field) | avg(field) | min(field) | max(field)>
[order by <field> [asc|desc]]
[limit <number>]
```

### Select Options & Aggregations
- **Specific fields:** `select name, email`
- **All fields:** `select *`
- **Aggregations:** `select count, sum(score), avg(age), min(price), max(price)`
  *(Note: Aggregations should be used alongside a `group by` clause for bucketed data analysis).*

### Built-In Inline Functions
You can manipulate strings and numbers on the fly inside `select` or `where` clauses:
- `upper(field)`: Converts text to uppercase.
- `lower(field)`: Converts text to lowercase.
- `concat(field1, field2)`: Joins two fields together.
- `length(field)`: Returns the character length of the field.
- `abs(field)`: Returns the absolute mathematical value.
- `round(field)`: Rounds a float to the nearest integer.

*Example:* `from users.csv join orders.csv on id == user_id where length(password) < 8 select upper(name), round(price)`

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

## CLI Configuration & Limits

QLE has strict security and execution limits to ensure the engine runs safely on embedded and resource-constrained environments. However, for production workloads or massive datasets, you can override these limits on the fly using CLI flags:

- `--max-file-size <bytes>`: By default, QLE caps files at 100MB. Use this flag to increase the allowed size of input files.
- `--max-rows <number>`: By default, QLE stops execution after processing 1,000,000 rows. Use this to allow scanning larger datasets.
- `--timeout <ms>`: By default, queries are terminated after 30 seconds (30,000ms). Increase this if running very complex aggregation queries on large files.

*Example:*
```bash
./qle --max-file-size 5368709120 --timeout 60000 "from huge_data.csv select count"
```
