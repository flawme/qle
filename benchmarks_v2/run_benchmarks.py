import os
import subprocess
import time
import duckdb
import pandas as pd
import json
import psutil

# Sizes to test
SIZES = {
    "1M": 1_000_000,
    "10M": 10_000_000
}

def generate_data(rows, filename, format_type):
    print(f"Generating {rows} rows for {filename}...")
    if format_type == "csv":
        with open(filename, "w") as f:
            f.write("id,name,age,score,date\n")
            # Write in chunks
            for chunk in range(rows // 100000):
                lines = []
                for i in range(100000):
                    idx = chunk * 100000 + i
                    lines.append(f"{idx},User{idx},{idx%100},{idx%1000},2023-05-15\n")
                f.writelines(lines)
    elif format_type == "json":
        with open(filename, "w") as f:
            f.write("[\n")
            for chunk in range(rows // 100000):
                lines = []
                for i in range(100000):
                    idx = chunk * 100000 + i
                    lines.append(f'{{"id":"{idx}","name":"User{idx}","age":"{idx%100}","score":"{idx%1000}","date":"2023-05-15"}}')
                    if not (chunk == rows // 100000 - 1 and i == 99999):
                        lines[-1] += ",\n"
                    else:
                        lines[-1] += "\n"
                f.writelines(lines)
            f.write("]\n")

def run_cmd(cmd):
    # Using /usr/bin/time to get peak RAM. -v gives verbose, max resident set size is in KB.
    time_cmd = ["/usr/bin/time", "-v"] + cmd
    start = time.time()
    try:
        res = subprocess.run(time_cmd, capture_output=True, text=True, timeout=120)
        dur = time.time() - start
        peak_ram_kb = 0
        for line in res.stderr.split("\n"):
            if "Maximum resident set size (kbytes):" in line:
                peak_ram_kb = int(line.split(":")[1].strip())
        return dur, peak_ram_kb / 1024.0 # return MB
    except subprocess.TimeoutExpired:
        return "TIMEOUT", "TIMEOUT"
    except Exception as e:
        return "ERROR", "ERROR"

def run_duckdb(query):
    start = time.time()
    try:
        duckdb.execute(query).fetchall()
        dur = time.time() - start
        # DuckDB uses process RAM, harder to measure peak precisely from python without external tools.
        # We will approximate or just note it's in-process. We'll use psutil.
        mem = psutil.Process(os.getpid()).memory_info().rss / (1024 * 1024)
        return dur, mem
    except Exception as e:
        return "ERROR", "ERROR"

def run_pandas(csv_file):
    start = time.time()
    try:
        df = pd.read_csv(csv_file)
        c = len(df[df["age"] > 50])
        dur = time.time() - start
        mem = psutil.Process(os.getpid()).memory_info().rss / (1024 * 1024)
        return dur, mem
    except Exception as e:
        return "ERROR", "ERROR"

def benchmark(name, cmd_type, cmd, file_size_mb):
    times = []
    rams = []
    print(f"  Testing {name}...")
    for _ in range(3):
        if cmd_type == "qle":
            t, r = run_cmd(["../build/qle", "--max-rows", "100000000", "--max-file-size", "10000000000", cmd])
        elif cmd_type == "awk":
            t, r = run_cmd(["awk", "-F,", "NR>1 && $3 > 50 {c++} END {print c}", cmd])
        elif cmd_type == "jq":
            t, r = run_cmd(["jq", "map(select(.age | tonumber > 50)) | length", cmd])
        elif cmd_type == "duckdb":
            t, r = run_duckdb(cmd)
        elif cmd_type == "pandas":
            t, r = run_pandas(cmd)
            
        if t == "TIMEOUT" or t == "ERROR":
            return t, r
        times.append(t)
        rams.append(r)
    
    avg_t = sum(times)/len(times)
    avg_r = sum(rams)/len(rams)
    return avg_t, avg_r

def main():
    results = []
    
    for label, rows in SIZES.items():
        csv_file = f"data_{label}.csv"
        json_file = f"data_{label}.json"
        
        if not os.path.exists(csv_file):
            generate_data(rows, csv_file, "csv")
        if not os.path.exists(json_file):
            generate_data(rows, json_file, "json")
            
        csv_mb = os.path.getsize(csv_file) / (1024*1024)
        json_mb = os.path.getsize(json_file) / (1024*1024)
        
        print(f"--- Benchmark Dataset {label} ---")
        
        # 1. CSV Filtering
        print("CSV Filtering (age > 50)")
        t, r = benchmark("QLE", "qle", f"from {csv_file} where age > 50 select count(id)", csv_mb)
        results.append((label, "CSV Filter", "QLE", t, r, csv_mb))
        
        t, r = benchmark("DuckDB", "duckdb", f"SELECT COUNT(*) FROM read_csv_auto('{csv_file}') WHERE age > 50", csv_mb)
        results.append((label, "CSV Filter", "DuckDB", t, r, csv_mb))
        
        t, r = benchmark("Pandas", "pandas", csv_file, csv_mb)
        results.append((label, "CSV Filter", "Pandas", t, r, csv_mb))
        
        t, r = benchmark("AWK", "awk", csv_file, csv_mb)
        results.append((label, "CSV Filter", "AWK", t, r, csv_mb))
        
        # 2. JSON Filtering
        print("JSON Filtering (age > 50)")
        t, r = benchmark("QLE", "qle", f"from {json_file} where age > 50 select count(id)", json_mb)
        results.append((label, "JSON Filter", "QLE", t, r, json_mb))
        
        # JQ will likely OOM on 10M, but we'll try it
        t, r = benchmark("jq", "jq", json_file, json_mb)
        results.append((label, "JSON Filter", "jq", t, r, json_mb))
        
        # 3. CSV Group By Sum
        print("CSV Group By Sum")
        t, r = benchmark("QLE", "qle", f"from {csv_file} group by age select sum(score)", csv_mb)
        results.append((label, "CSV GroupBy", "QLE", t, r, csv_mb))
        
        t, r = benchmark("DuckDB", "duckdb", f"SELECT age, SUM(score) FROM read_csv_auto('{csv_file}') GROUP BY age", csv_mb)
        results.append((label, "CSV GroupBy", "DuckDB", t, r, csv_mb))
        
    # Write Markdown
    with open("RESULTS.md", "w") as f:
        f.write("# Comparative Benchmarks: QLE vs Established Tools\n\n")
        f.write("## Test Environment\n")
        f.write("- All times are averaged across 3 runs.\n")
        f.write("- Peak RAM is measured using `/usr/bin/time -v` for CLI tools, and `psutil` for Python processes.\n\n")
        
        f.write("## Results\n\n")
        f.write("| Dataset Size | Test | Tool | Time (seconds) | Peak RAM (MB) | File Size (MB) |\n")
        f.write("|--------------|------|------|----------------|---------------|----------------|\n")
        for r in results:
            label, test, tool, t, ram, sz = r
            ts = f"{t:.2f} s" if isinstance(t, float) else str(t)
            rs = f"{ram:.1f} MB" if isinstance(ram, float) else str(ram)
            f.write(f"| {label} | {test} | {tool} | {ts} | {rs} | {sz:.1f} MB |\n")

if __name__ == "__main__":
    main()
