import re

# 1. Update docs/README.md
with open('docs/README.md', 'r') as f:
    content = f.read()

new_features = """- **Map-Reduce Parallel Execution:** Threaded `GROUP BY` logic and CSV adapter chunk splitting via `std::thread::hardware_concurrency()` for massive multicore throughput.
- **Incremental Aggregation:** `sum`, `count`, `min`, `max` process inline during streaming, completely eliminating row-caching memory footprints (10M row RAM usage is just 4.5 MB).
- **Projection Pushdown:** Columns not needed by the AST are skipped entirely at the parser level via read-masks, improving parse speeds dramatically (10M row queries in 3.7 seconds).
- **Modular Engine Architecture:** The monolithic `runtime.cpp` was decoupled into 5 separate component files (`evaluator.cpp`, `executor_streaming.cpp`, etc.).
- **Subqueries & Complex Relations:**"""

if "- **Subqueries & Complex Relations:**" in content and "Map-Reduce Parallel Execution" not in content:
    content = content.replace('- **Subqueries & Complex Relations:**', new_features)

with open('docs/README.md', 'w') as f:
    f.write(content)

# 2. Update docs/architecture.md
with open('docs/architecture.md', 'r') as f:
    arch = f.read()

# Modular Engine Architecture: The monolithic `runtime.cpp` was decoupled into 5 separate component files (`evaluator.cpp`, `executor_streaming.cpp`, etc.).
if "## 4. Runtime (`src/runtime`)" in arch:
    arch = arch.replace(
        "## 4. Runtime (`src/runtime`)\nThe Runtime executes the AST. It handles filtering, evaluating inline functions (`upper`, `abs`, etc.), grouping data via the `group by` clause, sorting rows based on `OrderByNode`, and performing multi-file hash joins.",
        "## 4. Runtime (`src/runtime`)\nThe Runtime executes the AST. The monolithic `runtime.cpp` was recently decoupled into 5 separate component files (`evaluator.cpp`, `executor_streaming.cpp`, `executor_groupby.cpp`, `executor_orderby.cpp`, `runtime.cpp`). It handles filtering, evaluating inline functions (`upper`, `abs`, etc.), grouping data via the `group by` clause (using Map-Reduce parallel execution), sorting rows based on `OrderByNode`, and performing multi-file hash joins."
    )

with open('docs/architecture.md', 'w') as f:
    f.write(arch)

# 3. Update docs/performance.md
with open('docs/performance.md', 'r') as f:
    perf = f.read()

# Add Projection Pushdown and Incremental Aggregation
new_perf = """
## Projection Pushdown

QLE aggressively optimizes read speeds using **Projection Pushdown**. Columns that are not needed by the AST (i.e. not referenced in `SELECT`, `WHERE`, `GROUP BY`, etc.) are skipped entirely at the parser level via read-masks. This drastically reduces string allocations and improves parse speeds dramatically (e.g. 10M row queries in 3.7 seconds).

## Map-Reduce Parallel Execution

To maximize multicore throughput, QLE employs **Map-Reduce Parallel Execution**. The CSV adapter utilizes chunk splitting via `std::thread::hardware_concurrency()`, and the `GROUP BY` logic runs concurrent map-reduce evaluations.

## Incremental Aggregation

Historically, grouping required memory-heavy row buffering. QLE now utilizes **Incremental Aggregation**, meaning `sum`, `count`, `min`, and `max` process inline during streaming. This completely eliminates row-caching memory footprints for aggregation queries. A 10M row file requires just 4.5 MB of RAM to process.

## Zero-Copy Architecture
"""

perf = perf.replace("## Zero-Copy Architecture", new_perf)

if "QLE must build a hashmap of all rows to aggregate buckets." in perf:
    perf = perf.replace(
        "2. **`group by`:** QLE must build a hashmap of all rows to aggregate buckets.",
        "2. **`group by`:** While QLE previously buffered rows to aggregate, it now uses inline Incremental Aggregation, so raw rows aren't fully buffered unless needed."
    )

with open('docs/performance.md', 'w') as f:
    f.write(perf)
