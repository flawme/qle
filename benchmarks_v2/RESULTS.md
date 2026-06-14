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
