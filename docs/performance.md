# Performance & Optimization Guide

QLE is designed from the ground up to be a highly secure, memory-safe, and blazing-fast data exploration engine. 

While the engine operates over raw flat files and does not use heavy index-based structures like PostgreSQL or MySQL, it relies on aggressive C++17 paradigms to achieve its speed.

## Zero-Copy Architecture

When processing large datasets (like multi-gigabyte CSVs), memory allocations can quickly bottleneck performance. To circumvent this, QLE utilizes **Zero-Copy Parsing**.

Instead of copying strings (`std::string`) for every cell of every row, QLE's `CsvAdapter` and core lexer operate on `std::string_view`. This means QLE scans the underlying memory buffer and references sub-sections of it directly, resulting in nearly zero intermediate string allocations during execution.

## Memory Streaming

By default, QLE **streams** rows directly from the source adapter to the console output. 

When you run a standard filtering query:
```text
from huge_data.csv where age > 18 select name, age limit 100
```
QLE processes and immediately discards rows one by one. This allows you to query a 50GB file on a laptop with less than 10MB of active RAM.

### When does QLE buffer data?
QLE only buffers data into RAM when the query requires full dataset context before outputting:
1. **`order by`:** QLE must load all matching rows into memory to sort them.
2. **`group by`:** QLE must build a hashmap of all rows to aggregate buckets.
3. **`--format table`:** QLE buffers the output to calculate the maximum width of each column for perfectly aligned borders.
4. **Hash Joins:** QLE builds an $O(1)$ memory map of the secondary data source to execute cross-file `join` queries seamlessly without disk-thrashing.

## O(1) Lookups & Cache Locality

QLE's internal engines strictly avoid expensive tree-based maps (`std::map`). 
- **Row Mapping:** Instead of mapping column names to values in a red-black tree, QLE uses heavily-optimized flat linear structures (like `std::vector<std::pair>`). Since data rows usually only contain a few dozen columns, $O(N)$ linear scans on flat arrays wildly outperform the cache misses associated with $O(\log N)$ tree lookups.
- **Aggregations:** The `group by` engine relies heavily on constant-time $O(1)$ lookups utilizing `std::unordered_map` to bucket results at lightning speed.

## Security Bounds vs. Production

To ensure QLE never crashes or locks up an embedded system, it implements strict bounds (e.g., maximum recursion limits, hard execution timeouts, and caps on maximum rows processed). 

If you are querying massive datasets in a secure production environment, you should override these caps using the CLI configuration flags:
- `--max-file-size`
- `--max-rows`
- `--timeout`
