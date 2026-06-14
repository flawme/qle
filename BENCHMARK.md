# QLE Comprehensive Benchmarks

## Test Environment

- **CPU:** AMD Ryzen 7 7445HS w/ Radeon 740M Graphics
- **RAM:** 14Gi
- **OS:** Linux 6.17.0-35-generic
- **Dataset Size:** 100,000 rows per primary file

## Results

| Feature Category | Test Case | Query | Execution Time |
|-----------------|-----------|-------|----------------|
| Adapters | CSV Parsing | `from bench.csv select count(id)` | **183.547 ms** |
| Adapters | JSON Parsing | `from bench.json select count(id)` | **208.531 ms** |
| Adapters | SQLite Parsing | `from bench.sqlite select count(id)` | **188.785 ms** |
| Adapters | XML Parsing | `from bench.xml select count(id)` | **283.174 ms** |
| Operations | Filtering (Numeric) | `from bench.csv where age > 50 select count(id)` | **207.396 ms** |
| Operations | Filtering (String) | `from bench.csv where name == "User50000" select count(id)` | **191.492 ms** |
| Operations | LIKE Pattern | `from bench.csv where name like "User1%0" select count(id)` | **202.183 ms** |
| Aggregations | GROUP BY + SUM | `from bench.csv group by age select sum(score)` | **239.951 ms** |
| Aggregations | GROUP BY + MIN/MAX | `from bench.csv group by age select min(score), max(score)` | **288.398 ms** |
| Sorting | ORDER BY (Numeric) | `from bench.csv select id, score order by score desc limit 10` | **722.144 ms** |
| Sorting | ORDER BY (String) | `from bench.csv select id, name order by name asc limit 10` | **488.583 ms** |
| Functions | String Functions | `from bench.csv select upper(name), concat(name, age) limit 1000` | **4.85012 ms** |
| Functions | Math Functions | `from bench.csv select abs(score), round(age) limit 1000` | **8.83214 ms** |
| Functions | Date Functions | `from bench.csv select now(), year(date), month(date) limit 1000` | **13.4761 ms** |
| Advanced | Subqueries | `from (from bench.csv where age > 80 select id, name) select count(id)` | **231.506 ms** |
| Advanced | Multiple Hash Joins | `from join1.csv join join2.csv on id == id join join3.csv on id == id select count(id)` | **9.67882 ms** |
