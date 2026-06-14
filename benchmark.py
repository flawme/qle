import os
import subprocess
import platform
import time
import sqlite3

ROWS = 100000

def generate_data():
    print("Generating data...")
    # CSV
    with open("bench.csv", "w") as f:
        f.write("id,name,age,score,date\n")
        for i in range(ROWS):
            f.write(f"{i},User{i},{i%100},{i%1000},2023-05-15\n")
            
    # JSON
    with open("bench.json", "w") as f:
        f.write("[\n")
        for i in range(ROWS):
            f.write(f'{{"id":"{i}","name":"User{i}","age":"{i%100}","score":"{i%1000}","date":"2023-05-15"}}')
            if i < ROWS - 1: f.write(",\n")
        f.write("\n]\n")

    # XML
    with open("bench.xml", "w") as f:
        f.write("<?xml version=\"1.0\"?>\n<rows>\n")
        for i in range(ROWS):
            f.write(f"  <row><id>{i}</id><name>User{i}</name><age>{i%100}</age><score>{i%1000}</score><date>2023-05-15</date></row>\n")
        f.write("</rows>\n")
        
    # SQLite
    if os.path.exists("bench.sqlite"): os.remove("bench.sqlite")
    conn = sqlite3.connect("bench.sqlite")
    c = conn.cursor()
    c.execute("CREATE TABLE users (id TEXT, name TEXT, age TEXT, score TEXT, date TEXT)")
    batch = []
    for i in range(ROWS):
        batch.append((str(i), f"User{i}", str(i%100), str(i%1000), "2023-05-15"))
        if len(batch) >= 10000:
            c.executemany("INSERT INTO users VALUES (?,?,?,?,?)", batch)
            batch = []
    if batch: c.executemany("INSERT INTO users VALUES (?,?,?,?,?)", batch)
    conn.commit()
    conn.close()

    # Smaller files for Joins
    with open("join1.csv", "w") as f:
        f.write("id,val1\n")
        for i in range(10000): f.write(f"{i},A{i}\n")
    with open("join2.csv", "w") as f:
        f.write("id,val2\n")
        for i in range(10000): f.write(f"{i},B{i}\n")
    with open("join3.csv", "w") as f:
        f.write("id,val3\n")
        for i in range(10000): f.write(f"{i},C{i}\n")

def run_query(query):
    # Ensure binary is fresh
    cmd = ["./build/qle", "--time", query]
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
        out = res.stderr
        for line in out.splitlines():
            if "Execution time:" in line:
                return line.split("Execution time:")[1].strip()
        return "N/A"
    except subprocess.TimeoutExpired:
        return "Timeout (10s)"
    except Exception as e:
        return f"Error: {e}"

def get_sys_info():
    try:
        cpu = subprocess.check_output("lscpu | grep 'Model name'", shell=True).decode().split(":")[1].strip()
    except:
        cpu = platform.processor()
    try:
        ram = subprocess.check_output("free -h | grep Mem", shell=True).decode().split()[1]
    except:
        ram = "Unknown"
    return cpu, ram, platform.system(), platform.release()

def main():
    generate_data()
    
    md = ["# QLE Comprehensive Benchmarks\n"]
    cpu, ram, os_name, os_rel = get_sys_info()
    md.append("## Test Environment\n")
    md.append(f"- **CPU:** {cpu}")
    md.append(f"- **RAM:** {ram}")
    md.append(f"- **OS:** {os_name} {os_rel}")
    md.append(f"- **Dataset Size:** {ROWS:,} rows per primary file\n")
    
    md.append("## Results\n")
    md.append("| Feature Category | Test Case | Query | Execution Time |")
    md.append("|-----------------|-----------|-------|----------------|")

    tests = [
        ("Adapters", "CSV Parsing", f"from bench.csv select count"),
        ("Adapters", "JSON Parsing", f"from bench.json select count"),
        ("Adapters", "SQLite Parsing", f"from bench.sqlite select count"),
        ("Adapters", "XML Parsing", f"from bench.xml select count"),
        
        ("Operations", "Filtering (Numeric)", f"from bench.csv where age > 50 select count"),
        ("Operations", "Filtering (String)", f"from bench.csv where name == \"User50000\" select count"),
        ("Operations", "LIKE Pattern", f"from bench.csv where name like \"User1%0\" select count"),
        
        ("Aggregations", "GROUP BY + SUM", f"from bench.csv group by age select sum(score)"),
        ("Aggregations", "GROUP BY + MIN/MAX", f"from bench.csv group by age select min(score), max(score)"),
        
        ("Sorting", "ORDER BY (Numeric)", f"from bench.csv order by score desc limit 10"),
        ("Sorting", "ORDER BY (String)", f"from bench.csv order by name asc limit 10"),
        
        ("Functions", "String Functions", f"from bench.csv select upper(name), concat(name, age) limit 1000"),
        ("Functions", "Math Functions", f"from bench.csv select abs(score), round(age) limit 1000"),
        ("Functions", "Date Functions", f"from bench.csv select now(), year(date), month(date) limit 1000"),
        
        ("Advanced", "Subqueries", f"from (from bench.csv where age > 80 select id, name) select count"),
        ("Advanced", "Multiple Hash Joins", f"from join1.csv join join2.csv on id == id join join3.csv on id == id select count"),
    ]
    
    for category, name, query in tests:
        print(f"Running {name}...")
        t = run_query(query)
        md.append(f"| {category} | {name} | `{query}` | **{t}** |")

    with open("BENCHMARK.md", "w") as f:
        f.write("\n".join(md) + "\n")
    print("Benchmarks complete! Saved to BENCHMARK.md")

if __name__ == "__main__":
    main()
