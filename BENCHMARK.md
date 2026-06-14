# QLE Comprehensive Benchmarks

## Test Environment

- **CPU:** AMD Ryzen 7 7445HS w/ Radeon 740M Graphics
- **RAM:** 14Gi
- **OS:** Linux 6.17.0-35-generic
- **Dataset Size:** 100,000 rows per primary file

## Results

| Feature Category | Test Case | Query | Execution Time |
|-----------------|-----------|-------|----------------|
| Adapters | CSV Parsing | `from bench.csv select count(id)` | **233 ms** |
| Adapters | JSON Parsing | `from bench.json select count(id)` | **386 ms** |
| Adapters | SQLite Parsing | `from bench.sqlite select count(id)` | **404 ms** |
| Adapters | XML Parsing | `from bench.xml select count(id)` | **563 ms** |
| Operations | Filtering (Numeric) | `from bench.csv where age > 50 select count(id)` | **269 ms** |
| Operations | Filtering (String) | `from bench.csv where name == "User50000" select count(id)` | **439 ms** |
| Operations | LIKE Pattern | `from bench.csv where name like "User1%0" select count(id)` | **222 ms** |
| Aggregations | GROUP BY + SUM | `from bench.csv group by age select sum(score)` | **377 ms** |
| Aggregations | GROUP BY + MIN/MAX | `from bench.csv group by age select min(score), max(score)` | **386 ms** |
| Sorting | ORDER BY (Numeric) | `from bench.csv order by score desc limit 10` | **0 ms** |
| Sorting | ORDER BY (String) | `from bench.csv order by name asc limit 10` | **0 ms** |
| Functions | String Functions | `from bench.csv select upper(name), concat(name, age) limit 1000` | **14 ms** |
| Functions | Math Functions | `from bench.csv select abs(score), round(age) limit 1000` | **8 ms** |
| Functions | Date Functions | `from bench.csv select now(), year(date), month(date) limit 1000` | **11 ms** |
| Advanced | Subqueries | `from (from bench.csv where age > 80 select id, name) select count(id)` | **281 ms** |
| Advanced | Multiple Hash Joins | `from join1.csv join join2.csv on id == id join join3.csv on id == id select count(id)` | **16 ms** |

# Comparative Benchmarks: QLE vs Established Tools

## Test Environment
- All times are averaged across 3 runs.
- Peak RAM is measured using `/usr/bin/time -v` for CLI tools, and `psutil` for Python processes.

## Results

| Dataset Size | Test | Tool | Time (seconds) | Peak RAM (MB) | File Size (MB) |
|--------------|------|------|----------------|---------------|----------------|
| 1M | CSV Filter | QLE | 3.55 s | 287.8 MB | 33.9 MB |
| 1M | CSV Filter | DuckDB | 0.06 s | 144.2 MB | 33.9 MB |
| 1M | CSV Filter | Pandas | 0.48 s | 299.4 MB | 33.9 MB |
| 1M | CSV Filter | AWK | 0.14 s | 2.7 MB | 33.9 MB |
| 1M | JSON Filter | QLE | 3.89 s | 365.5 MB | 77.8 MB |
| 1M | JSON Filter | jq | 2.42 s | 754.5 MB | 77.8 MB |
| 1M | CSV GroupBy | QLE | 4.81 s | 585.7 MB | 33.9 MB |
| 1M | CSV GroupBy | DuckDB | 0.07 s | 264.7 MB | 33.9 MB |
| 10M | CSV Filter | QLE | 31.12 s | 2627.6 MB | 358.3 MB |
| 10M | CSV Filter | DuckDB | 0.25 s | 655.3 MB | 358.3 MB |
| 10M | CSV Filter | Pandas | 4.54 s | 1701.0 MB | 358.3 MB |
| 10M | CSV Filter | AWK | 1.39 s | 2.8 MB | 358.3 MB |
| 10M | JSON Filter | QLE | 31.04 s | 3045.5 MB | 797.0 MB |
| 10M | JSON Filter | jq | 24.33 s | 7435.0 MB | 797.0 MB |
| 10M | CSV GroupBy | QLE | 35.54 s | 4801.2 MB | 358.3 MB |
| 10M | CSV GroupBy | DuckDB | 0.28 s | 875.4 MB | 358.3 MB |
