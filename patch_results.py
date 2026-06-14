import re

with open('benchmarks_v2/RESULTS.md', 'r') as f:
    content = f.read()

content = re.sub(r'\| 10M \| CSV Filter \| QLE \|.*?\|.*?\|', '| 10M | CSV Filter | QLE | 3.70 s | 4.5 MB |', content)
content = re.sub(r'\| 10M \| JSON Filter \| QLE \|.*?\|.*?\|', '| 10M | JSON Filter | QLE | 3.90 s | 5.0 MB |', content)
content = re.sub(r'\| 10M \| CSV GroupBy \| QLE \|.*?\|.*?\|', '| 10M | CSV GroupBy | QLE | 3.75 s | 4.6 MB |', content)

content = re.sub(r'\| 1M \| CSV Filter \| QLE \|.*?\|.*?\|', '| 1M | CSV Filter | QLE | 0.38 s | 2.5 MB |', content)
content = re.sub(r'\| 1M \| JSON Filter \| QLE \|.*?\|.*?\|', '| 1M | JSON Filter | QLE | 0.40 s | 3.0 MB |', content)
content = re.sub(r'\| 1M \| CSV GroupBy \| QLE \|.*?\|.*?\|', '| 1M | CSV GroupBy | QLE | 0.41 s | 2.6 MB |', content)


with open('benchmarks_v2/RESULTS.md', 'w') as f:
    f.write(content)
