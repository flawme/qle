import os
import subprocess

ROWS = 10000000

print(f"Generating {ROWS} rows...")
with open("large.csv", "w") as f:
    f.write("id,name,age,score,date\n")
    for i in range(ROWS):
        f.write(f"{i},User{i},{i%100},{i%1000},2023-05-15\n")

print("Running filter query...")
cmd = "/usr/bin/time -v ./build/qle 'from large.csv where age > 50 select count(id)' > /dev/null"
res = subprocess.run(cmd, shell=True, capture_output=True, text=True)
for line in res.stderr.splitlines():
    if "Maximum resident set size" in line:
        print("Filter RAM:", line.strip())

print("Running groupby query...")
cmd = "/usr/bin/time -v ./build/qle 'from large.csv group by age select sum(score)' > /dev/null"
res = subprocess.run(cmd, shell=True, capture_output=True, text=True)
for line in res.stderr.splitlines():
    if "Maximum resident set size" in line:
        print("GroupBy RAM:", line.strip())
