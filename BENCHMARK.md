# QLE Comprehensive Benchmarks

## Test Environment

- **CPU:** AMD Ryzen 7 7445HS w/ Radeon 740M Graphics
- **RAM:** 14Gi
- **OS:** Linux 6.17.0-35-generic
- **Dataset Size:** 100,000 rows per primary file

## Results

| Feature Category | Test Case | Query | Execution Time |
|-----------------|-----------|-------|----------------|
| Adapters | CSV Parsing | `from bench.csv select count` | **0 ms** |
| Adapters | JSON Parsing | `from bench.json select count` | **25 ms** |
| Adapters | SQLite Parsing | `from bench.sqlite select count` | **0 ms** |
| Adapters | XML Parsing | `from bench.xml select count` | **0 ms** |
| Operations | Filtering (Numeric) | `from bench.csv where age > 50 select count` | **1 ms** |
| Operations | Filtering (String) | `from bench.csv where name == "User50000" select count` | **251 ms** |
| Operations | LIKE Pattern | `from bench.csv where name like "User1%0" select count` | **0 ms** |
| Aggregations | GROUP BY + SUM | `from bench.csv group by age select sum(score)` | **466 ms** |
| Aggregations | GROUP BY + MIN/MAX | `from bench.csv group by age select min(score), max(score)` | **446 ms** |
| Sorting | ORDER BY (Numeric) | `from bench.csv order by score desc limit 10` | **0 ms** |
| Sorting | ORDER BY (String) | `from bench.csv order by name asc limit 10` | **0 ms** |
| Functions | String Functions | `from bench.csv select upper(name), concat(name, age) limit 1000` | **11 ms** |
| Functions | Math Functions | `from bench.csv select abs(score), round(age) limit 1000` | **13 ms** |
| Functions | Date Functions | `from bench.csv select now(), year(date), month(date) limit 1000` | **11 ms** |
| Advanced | Subqueries | `from (from bench.csv where age > 80 select id, name) select count` | **342 ms** |
| Advanced | Multiple Hash Joins | `from join1.csv join join2.csv on id == id join join3.csv on id == id select count` | **44 ms** |
