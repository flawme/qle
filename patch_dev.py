import re

with open('docs/development.md', 'r') as f:
    dev = f.read()

dev = dev.replace('src/runtime/runtime.cpp', 'src/runtime/runtime.cpp (or the relevant decoupled component like evaluator/executor)')

new_arch_note = """
## Modular Architecture

The execution engine has been cleanly decoupled from a monolithic structure into 5 distinct component files (`evaluator.cpp`, `executor_streaming.cpp`, `executor_groupby.cpp`, `executor_orderby.cpp`, `runtime.cpp`). This separation of concerns allows developers to isolate scaling changes (like Map-Reduce threading logic in `executor_groupby.cpp`) without impacting the overall `Runtime` state machine.
"""

if "Modular Architecture" not in dev:
    dev += new_arch_note

with open('docs/development.md', 'w') as f:
    f.write(dev)
