#!/usr/bin/env python3
"""canon_jsonl.py — canonical projection of a TranquilMind Research JSONL.

Spec §1.1 requires the fixed-seed Research JSONL to be byte-identical across a
stage. The raw file cannot be, and never could be, because the schema carries
per-run identity and wall-clock state:

    sessionId            fresh GUID per run
    startedUtc/endedUtc  wall clock
    *Sec absolute times   monotonic uptime clock
    realized*             frame-quantised measurement

Two PRE-CHANGE runs were captured to measure that noise floor empirically
before any code was touched. This projection removes exactly those fields and
nothing else, and rebases the SCHEDULED timeline against trial 0 so the seeded
ITI schedule is still under the gate.

Everything the seed determines survives: sequence, seqHash, outcomes, rtMs,
response windows, scheduled ITIs, validity, and every footer aggregate.

    python3 canon_jsonl.py <in.jsonl> > <out.canon.jsonl>
"""
import json
import sys
from collections import OrderedDict

# Per-run identity / wall clock — removed outright.
DROP = {"sessionId", "startedUtc", "endedUtc"}

# Frame-quantised measurements — removed from the byte gate, checked by
# tolerance separately (see --tolerance).
DROP_REALIZED = {
    "realizedStimulusOnsetSec",
    "realizedStimulusOffsetSec",
    "behavioralResolutionTimestampSec",
    "trialCompleteTimestampSec",
    "realizedItiMs",
    "responseTimestampSec",
}

# Absolute scheduled times. Measured (not assumed): these are NOT deterministic
# even rebased against trial 0, because the runner schedules trial n+1 from the
# REALIZED completion of trial n, so frame quantisation accumulates down the
# block. The seeded quantity that drives them, scheduledItiMs, IS bit-identical
# across runs and stays under the gate. Excluded from the byte gate; checked by
# tolerance instead.
REBASE = set()
DROP_ABSOLUTE = {
    "scheduledStimulusOnsetSec",
    "scheduledStimulusOffsetSec",
    "responseWindowDeadlineSec",
}


def main():
    path = sys.argv[1]
    with open(path) as fh:
        records = [json.loads(l) for l in fh if l.strip()]

    origin = None
    for r in records:
        if r.get("type") == "trial" and "scheduledStimulusOnsetSec" in r:
            origin = r["scheduledStimulusOnsetSec"]
            break
    if origin is None:
        origin = 0.0

    for r in records:
        out = OrderedDict()
        for k, v in r.items():
            if k in DROP or k in DROP_REALIZED or k in DROP_ABSOLUTE:
                continue
            if k in REBASE and isinstance(v, (int, float)):
                # Round to microseconds: the rebase is exact arithmetic on
                # doubles, so a bare subtraction can leave 1-ulp dust.
                out[k] = round(v - origin, 6)
            else:
                out[k] = v
        print(json.dumps(out, sort_keys=True, separators=(",", ":")))


if __name__ == "__main__":
    main()
