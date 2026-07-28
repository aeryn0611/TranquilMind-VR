#!/usr/bin/env python3
"""Field-level diff of two Research JSONL runs.

Reports, per field name, whether values are identical across two runs.
Used to establish which fields are deterministic (R5) and which are
wall-clock/frame noise, so a Stage 1 regression difference is interpretable.
"""
import json
import sys
from collections import OrderedDict

def load(p):
    with open(p) as fh:
        return [json.loads(l) for l in fh if l.strip()]

a, b = load(sys.argv[1]), load(sys.argv[2])

if len(a) != len(b):
    print(f"RECORD COUNT DIFFERS: {len(a)} vs {len(b)}")
    sys.exit(1)
print(f"record count identical: {len(a)}")

stable, unstable = OrderedDict(), OrderedDict()
for ra, rb in zip(a, b):
    if ra.get("type") != rb.get("type"):
        print(f"TYPE MISMATCH at record: {ra.get('type')} vs {rb.get('type')}")
        sys.exit(1)
    for k in ra:
        if k not in rb:
            unstable.setdefault(k, []).append(("<missing>", "<missing>"))
            continue
        if ra[k] == rb[k]:
            stable[k] = stable.get(k, 0) + 1
        else:
            unstable.setdefault(k, []).append((ra[k], rb[k]))

print("\n--- IDENTICAL across both runs ---")
for k, n in stable.items():
    print(f"  {k}  ({n} records)")

print("\n--- DIFFERS between runs (pre-change noise floor) ---")
for k, samples in unstable.items():
    s = samples[0]
    print(f"  {k}  ({len(samples)} records)  e.g. {s[0]!r} vs {s[1]!r}")
if not unstable:
    print("  (none — files are fully deterministic)")
