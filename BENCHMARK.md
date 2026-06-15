# QLE Comprehensive Benchmarks

## Test Environment

- **CPU:** AMD Ryzen 7 7445HS w/ Radeon 740M Graphics
- **RAM:** 14Gi
- **OS:** Linux 6.17.0-35-generic
- **Dataset Size:** 1,000,000 rows per primary file

## Results

| Feature Category | Test Case | Query | Execution Time |
|-----------------|-----------|-------|----------------|
| Adapters | CSV Parsing | `from bench.csv select count(id)` | **557.52 ms** |
| Adapters | JSON Parsing | `from bench.json select count(id)` | **539.187 ms** |
| Adapters | SQLite Parsing | `from bench.sqlite select count(id)` | **2538.58 ms** |
| Adapters | XML Parsing | `from bench.xml select count(id)` | **0.284368 ms** |
| Operations | Filtering (Numeric) | `from bench.csv where age > 50 select count(id)` | **674.614 ms** |
| Operations | Filtering (String) | `from bench.csv where name == "User50000" select count(id)` | **590.401 ms** |
| Operations | LIKE Pattern | `from bench.csv where name like "User1%0" select count(id)` | **639.069 ms** |
| Aggregations | GROUP BY + SUM | `from bench.csv group by age select sum(score)` | **694.351 ms** |
| Aggregations | GROUP BY + MIN/MAX | `from bench.csv group by age select min(score), max(score)` | **797.043 ms** |
| Sorting | ORDER BY (Numeric) | `from bench.csv select id, score order by score desc limit 10` | **2376.35 ms** |
| Sorting | ORDER BY (String) | `from bench.csv select id, name order by name asc limit 10` | **2506.15 ms** |
| Functions | String Functions | `from bench.csv select upper(name), concat(name, age) limit 1000` | **8.47401 ms** |
| Functions | Math Functions | `from bench.csv select abs(score), round(age) limit 1000` | **7.70188 ms** |
| Functions | Date Functions | `from bench.csv select now(), year(date), month(date) limit 1000` | **13.8666 ms** |
| Advanced | Subqueries | `from (from bench.csv where age > 80 select id, name) select count(id)` | **3471.61 ms** |
| Advanced | CTE (WITH) | `with OldUsers as (from bench.csv where age > 80) from OldUsers select count(id)` | **0.120778 ms** |
| Advanced | HAVING Clause | `from bench.csv group by age having sum(score) > 50000 select min(score)` | **805.099 ms** |
| Advanced | Multiple Hash Joins | `from join1.csv join join2.csv on id == id join join3.csv on id == id select count(id)` | **14.2576 ms** |
