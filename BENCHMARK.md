# QLE Comprehensive Benchmarks

## Test Environment

- **CPU:** AMD Ryzen 7 7445HS w/ Radeon 740M Graphics
- **RAM:** 14Gi
- **OS:** Linux 6.17.0-35-generic
- **Dataset Size:** 1,000,000 rows per primary file

## Results

| Feature Category | Test Case | Query | Execution Time |
|-----------------|-----------|-------|----------------|
| Adapters | CSV Parsing | `from bench.csv select count(id)` | **355.232 ms** |
| Adapters | JSON Parsing | `from bench.json select count(id)` | **452.474 ms** |
| Adapters | SQLite Parsing | `from bench.sqlite select count(id)` | **1855.93 ms** |
| Adapters | XML Parsing | `from bench.xml select count(id)` | **0.145233 ms** |
| Operations | Filtering (Numeric) | `from bench.csv where age > 50 select count(id)` | **363.966 ms** |
| Operations | Filtering (String) | `from bench.csv where name == "User50000" select count(id)` | **348.717 ms** |
| Operations | LIKE Pattern | `from bench.csv where name like "User1%0" select count(id)` | **355.505 ms** |
| Aggregations | GROUP BY + SUM | `from bench.csv group by age select sum(score)` | **387.515 ms** |
| Aggregations | GROUP BY + MIN/MAX | `from bench.csv group by age select min(score), max(score)` | **493.27 ms** |
| Sorting | ORDER BY (Numeric) | `from bench.csv select id, score order by score desc limit 10` | **3198.96 ms** |
| Sorting | ORDER BY (String) | `from bench.csv select id, name order by name asc limit 10` | **2307.8 ms** |
| Functions | String Functions | `from bench.csv select upper(name), concat(name, age) limit 1000` | **5.80156 ms** |
| Functions | Math Functions | `from bench.csv select abs(score), round(age) limit 1000` | **7.30192 ms** |
| Functions | Date Functions | `from bench.csv select now(), year(date), month(date) limit 1000` | **10.8532 ms** |
| Advanced | Subqueries | `from (from bench.csv where age > 80 select id, name) select count(id)` | **2353.11 ms** |
| Advanced | CTE (WITH) | `with OldUsers as (from bench.csv where age > 80) from OldUsers select count(id)` | **0.120817 ms** |
| Advanced | HAVING Clause | `from bench.csv group by age having sum(score) > 50000 select min(score)` | **400.592 ms** |
| Advanced | Multiple Hash Joins | `from join1.csv join join2.csv on id == id join join3.csv on id == id select count(id)` | **3.65359 ms** |
