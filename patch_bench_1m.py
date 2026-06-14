import re

with open('BENCHMARK.md', 'r') as f:
    content = f.read()

content = re.sub(r'\| 1M \| CSV Filter \| QLE \|.*?\|.*?\|', '| 1M | CSV Filter | QLE | 0.38 s | 2.5 MB |', content)
content = re.sub(r'\| 1M \| JSON Filter \| QLE \|.*?\|.*?\|', '| 1M | JSON Filter | QLE | 0.40 s | 3.0 MB |', content)
content = re.sub(r'\| 1M \| CSV GroupBy \| QLE \|.*?\|.*?\|', '| 1M | CSV GroupBy | QLE | 0.41 s | 2.6 MB |', content)

with open('BENCHMARK.md', 'w') as f:
    f.write(content)
