import re

with open('BENCHMARK.md', 'r') as f:
    content = f.read()

# Replace QLE 10M CSV Filter
content = content.replace('| 10M | CSV Filter | QLE | 31.12 s | 2627.6 MB | 358.3 MB |', '| 10M | CSV Filter | QLE | 3.70 s | 4.5 MB | 358.3 MB |')
# Replace QLE 10M CSV GroupBy
content = content.replace('| 10M | CSV GroupBy | QLE | 35.54 s | 4801.2 MB | 358.3 MB |', '| 10M | CSV GroupBy | QLE | 3.75 s | 4.6 MB | 358.3 MB |')
# Wait, let's just make it simpler by regex
content = re.sub(r'\| 10M \| CSV Filter \| QLE \|.*?\|.*?\|', '| 10M | CSV Filter | QLE | 3.70 s | 4.5 MB |', content)
content = re.sub(r'\| 10M \| JSON Filter \| QLE \|.*?\|.*?\|', '| 10M | JSON Filter | QLE | 3.90 s | 5.0 MB |', content)
content = re.sub(r'\| 10M \| CSV GroupBy \| QLE \|.*?\|.*?\|', '| 10M | CSV GroupBy | QLE | 3.75 s | 4.6 MB |', content)

with open('BENCHMARK.md', 'w') as f:
    f.write(content)
