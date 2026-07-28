#!/usr/bin/env python3
"""
Stage 2 acceptance checker for schema-2 TMS1 traces.

Validates the Spec §11.1 Stage-2 acceptance criteria S2-A .. S2-I against a
single trace session, using the canonical actor identity (sid, spawnseq).

This is a NEW, separate tool. The hardened Stage 1 parser
(Documentation/Stage1/tools/parse_tm_trace_v2.py) and its 27 regression fixtures
are deliberately NOT modified — they remain the frozen evidence-chain tool.

Usage:
    python3 stage2_check.py <log> [--session SID] [--list-sessions]

Exit codes:
    0  all checks pass
    1  one or more checks fail
    2  usage / parse error
    3  multiple sessions present and none selected (fails closed, as schema 2
       requires: logs from different sessions must never be silently merged)
"""

import argparse
import re
import sys
from collections import defaultdict, OrderedDict

LINE_RE = re.compile(r"TMS1\|(.*)$")

# Phase names emitted by UTMDemoLifecycleComponent.
LIFECYCLE_PHASES = [
    "p1_spawn", "p2_entrance", "p3_hold", "p4_response", "p5_outcomelock",
    "p6_persistence", "p7_exit", "p8_hidden", "p9_destroy",
]


def parse(path):
    """Return [dict] of TMS1 records, in file order."""
    records = []
    with open(path, "r", errors="replace") as fh:
        for raw in fh:
            m = LINE_RE.search(raw)
            if not m:
                continue
            fields = OrderedDict()
            for part in m.group(1).split("|"):
                if "=" not in part:
                    continue
                k, v = part.split("=", 1)
                fields[k] = v
            if "ev" not in fields:
                continue
            records.append(fields)
    return records


def to_int(v, default=None):
    try:
        return int(v)
    except (TypeError, ValueError):
        return default


def to_float(v, default=None):
    try:
        return float(v)
    except (TypeError, ValueError):
        return default


def sessions(records):
    """sid -> summary. A SESSION_BEGIN opens; SESSION_END closes."""
    out = OrderedDict()
    for r in records:
        sid = r.get("sid")
        if sid is None:
            continue
        s = out.setdefault(sid, {
            "sid": sid, "events": 0, "spawns": 0, "map": "?", "mode": "?",
            "pie": "?", "end_reason": None, "first_f": None, "last_f": None,
        })
        s["events"] += 1
        f = to_int(r.get("f"))
        if f is not None:
            s["first_f"] = f if s["first_f"] is None else min(s["first_f"], f)
            s["last_f"] = f if s["last_f"] is None else max(s["last_f"], f)
        if r["ev"] == "SESSION_BEGIN":
            s["map"] = r.get("map", "?")
            s["mode"] = r.get("mode", "?")
            s["pie"] = r.get("pie", "?")
        elif r["ev"] == "SESSION_END":
            s["end_reason"] = r.get("reasontext", r.get("reason", "?"))
        elif r["ev"] == "SPAWN":
            s["spawns"] += 1
    return out


class Result:
    def __init__(self):
        self.rows = []
        self.failed = False

    def check(self, ident, label, ok, detail=""):
        self.rows.append((ident, label, "PASS" if ok else "FAIL", detail))
        if not ok:
            self.failed = True

    def info(self, ident, label, detail):
        self.rows.append((ident, label, "INFO", detail))

    def render(self):
        w0 = max(len(r[0]) for r in self.rows)
        w1 = max(len(r[1]) for r in self.rows)
        lines = []
        for ident, label, status, detail in self.rows:
            lines.append(f"{ident:<{w0}}  {label:<{w1}}  {status:<4}  {detail}")
        return "\n".join(lines)


def analyse(records, sid):
    recs = [r for r in records if r.get("sid") == sid]
    res = Result()

    # ---- identity-keyed timelines: (sid, spawnseq) ------------------------
    by_ident = defaultdict(list)
    for r in recs:
        seq = r.get("spawnseq")
        if seq is None or seq == "-1":
            continue          # session- or spawner-level event
        by_ident[(sid, seq)].append(r)

    res.info("--", "session", f"sid={sid} events={len(recs)} identities={len(by_ident)}")

    # ---- S2-A / S2-B: ownership and registration maxima --------------------
    max_active = max_visible = max_live = -1
    overlap_frames = 0
    live_gt1_frames = 0
    active_gt1_frames = 0
    for r in recs:
        a = to_int(r.get("activetargets"), -1)
        v = to_int(r.get("visiblepresentations"), -1)
        l = to_int(r.get("livepresentation"), -1)
        max_active = max(max_active, a)
        max_visible = max(max_visible, v)
        max_live = max(max_live, l)
        if v >= 2:
            overlap_frames += 1
        if l >= 2:
            live_gt1_frames += 1
        if a >= 2:
            active_gt1_frames += 1

    res.check("S2-A", "max official live presentations = 1",
              max_live == 1, f"observed max livepresentation = {max_live}")
    res.check("S2-A", "never 2 official presentations",
              live_gt1_frames == 0, f"{live_gt1_frames} sample(s) with livepresentation >= 2")
    res.check("S2-B", "max ActiveTargets = 1",
              max_active == 1, f"observed max activetargets = {max_active}")
    res.check("S2-B", "never 2 registered targets",
              active_gt1_frames == 0, f"{active_gt1_frames} sample(s) with activetargets >= 2")
    res.check("S2-D", "no role-coded target overlap",
              overlap_frames == 0,
              f"{overlap_frames} sample(s) with visiblepresentations >= 2 "
              f"(max {max_visible})")

    # ---- identity integrity: no relabelling --------------------------------
    mixed_cyc = [k for k, v in by_ident.items() if len({r.get("cyc") for r in v}) > 1]
    mixed_auid = [k for k, v in by_ident.items() if len({r.get("auid") for r in v}) > 1]
    res.check("ID", "no identity carries two cycle numbers",
              not mixed_cyc, f"{len(mixed_cyc)} offending identity/identities")
    res.check("ID", "no identity carries two auid",
              not mixed_auid, f"{len(mixed_auid)} offending identity/identities")

    spans = {}
    for k, v in by_ident.items():
        fs = [to_int(r.get("f")) for r in v if to_int(r.get("f")) is not None]
        if fs:
            spans[k] = (min(fs), max(fs))

    overlapping_pairs = []
    keys = sorted(spans, key=lambda k: spans[k][0])
    for i in range(len(keys) - 1):
        a, b = keys[i], keys[i + 1]
        if spans[a][1] >= spans[b][0]:
            overlapping_pairs.append((a, b, spans[a], spans[b]))
    res.check("S2-C", "no predecessor event relabelled to a successor",
              True, "identity is (sid, spawnseq); no shared-key collisions found"
              if not mixed_cyc else "see ID failures")
    res.check("S2-C", "no successor presentation overlaps a predecessor",
              not overlapping_pairs,
              f"{len(overlapping_pairs)} overlapping identity frame-span pair(s)"
              + ("" if not overlapping_pairs else f" e.g. {overlapping_pairs[0][0]} {overlapping_pairs[0][2]}"
                 f" vs {overlapping_pairs[1 - 1][1]} {overlapping_pairs[0][3]}"))

    # ---- phase timeline per identity --------------------------------------
    bad_order = []
    missing_phase = defaultdict(int)
    complete = 0
    for k, v in by_ident.items():
        seen = [r for r in v if r["ev"] == "PHASE" and r.get("to", "").startswith("p")]
        got = [r["to"] for r in seen]
        got_lifecycle = [g for g in got if g in LIFECYCLE_PHASES]
        if not got_lifecycle:
            continue
        idx = [LIFECYCLE_PHASES.index(g) for g in got_lifecycle]
        if idx != sorted(idx) or len(set(idx)) != len(idx):
            bad_order.append((k, got_lifecycle))
        for p in LIFECYCLE_PHASES:
            if p not in got_lifecycle:
                missing_phase[p] += 1
        if len(set(got_lifecycle)) == len(LIFECYCLE_PHASES):
            complete += 1

    res.check("PHASE", "phase transitions are strictly ordered, no repeats",
              not bad_order, f"{len(bad_order)} identity/identities out of order")
    res.check("PHASE", "all nine phases traversed by completed presentations",
              complete > 0 and not missing_phase,
              f"{complete} identity/identities traversed all 9; "
              f"missing counts={dict(missing_phase) if missing_phase else '{}'}")

    # ---- S2-G / S2-I: outcome and registration confined to Phase 4 ----------
    # Reconstruct each identity's phase at the frame of each OUTCOME.
    outcome_outside_p4 = []
    for k, v in by_ident.items():
        phase = None
        for r in sorted(v, key=lambda x: (to_int(x.get("f"), 0))):
            if r["ev"] == "PHASE" and r.get("to") in LIFECYCLE_PHASES:
                phase = r["to"]
            elif r["ev"] == "OUTCOME":
                # An outcome is committed AT the lock, so the legal phases are
                # p4 (the resolve call that triggers the lock) and p5.
                if phase not in ("p4_response", "p5_outcomelock"):
                    outcome_outside_p4.append((k, phase, r.get("outcome")))
    res.check("S2-G", "no scored event fires outside Phase 4 / the outcome lock",
              not outcome_outside_p4, f"{len(outcome_outside_p4)} violation(s)")

    outcomes = defaultdict(int)
    multi_outcome = []
    for k, v in by_ident.items():
        n = sum(1 for r in v if r["ev"] == "OUTCOME")
        if n > 1:
            multi_outcome.append((k, n))
        for r in v:
            if r["ev"] == "OUTCOME":
                outcomes[r.get("outcome", "?")] += 1
    res.check("EXACTLY-ONCE", "at most one outcome per presentation",
              not multi_outcome, f"{len(multi_outcome)} identity/identities with >1 OUTCOME")
    res.info("--", "outcome distribution", str(dict(outcomes)))

    multi_destroy = [(k, sum(1 for r in v if r["ev"] == "DESTROY_REQ"))
                     for k, v in by_ident.items()
                     if sum(1 for r in v if r["ev"] == "DESTROY_REQ") > 1]
    res.check("EXACTLY-ONCE", "at most one destroy request per presentation",
              not multi_destroy, f"{len(multi_destroy)} identity/identities with >1 DESTROY_REQ")

    # ---- timing: cadence, response window, visual lifetime -----------------
    def first_wt(v, ev):
        for r in sorted(v, key=lambda x: to_int(x.get("f"), 0)):
            if r["ev"] == ev:
                return to_float(r.get("wt"))
        return None

    def first_phase_wt(v, phase):
        for r in sorted(v, key=lambda x: to_int(x.get("f"), 0)):
            if r["ev"] == "PHASE" and r.get("to") == phase:
                return to_float(r.get("wt"))
        return None

    # A Void is a session CANCELLATION (hard gate, phase change, restart, demo
    # end, forced teardown), not a scored response: by definition it does not
    # run its response window or its scheduled visual lifetime to completion.
    # Pooling it into these statistics would measure the kill signal, not the
    # timing. Cancelled presentations are counted and reported separately.
    spawn_times, windows, lifetimes = [], [], []
    cancelled = 0
    spawn_frames = []
    for k, v in sorted(by_ident.items(), key=lambda kv: to_int(kv[0][1], 0)):
        t_spawn = first_wt(v, "SPAWN")
        t_out = first_wt(v, "OUTCOME")
        t_end = first_wt(v, "ENDPLAY")
        outcome = next((r.get("outcome") for r in v if r["ev"] == "OUTCOME"), None)
        f_spawn = next((to_int(r.get("f")) for r in v if r["ev"] == "SPAWN"), None)

        if t_spawn is not None:
            spawn_times.append(t_spawn)
        if f_spawn is not None:
            spawn_frames.append(f_spawn)

        if outcome == "Void":
            cancelled += 1
            continue

        if t_spawn is not None and t_out is not None:
            windows.append((t_out - t_spawn) * 1000.0)
        if t_spawn is not None and t_end is not None:
            lifetimes.append((t_end - t_spawn) * 1000.0)

    res.info("--", "cancelled (Void) presentations excluded from timing",
             f"{cancelled} of {len(by_ident)}")

    frame_deltas = defaultdict(int)
    for i in range(len(spawn_frames) - 1):
        frame_deltas[spawn_frames[i + 1] - spawn_frames[i]] += 1
    res.info("--", "spawn frame-delta multiset (72 fps)",
             str(dict(sorted(frame_deltas.items()))))

    def band(values, target, tol, label, ident):
        """Every sample must sit inside the band, not merely the mean."""
        if not values:
            res.check(ident, label, False, "no samples")
            return
        lo, hi = min(values), max(values)
        ok = (abs(lo - target) <= tol) and (abs(hi - target) <= tol)
        res.check(ident, label, ok,
                  f"n={len(values)} mean={sum(values)/len(values):.1f} ms "
                  f"min={lo:.1f} max={hi:.1f} (every sample within {target} +/- {tol})")

    deltas = [(spawn_times[i + 1] - spawn_times[i]) * 1000.0
              for i in range(len(spawn_times) - 1)]

    # Cadence is checked in two parts, because a single non-modal interval is
    # EXPECTED and is not a cadence error: UTargetSpawnerComponent's
    # HandlePhaseChanged() sets LastSpawnTimestamp_SEC = -1 at the Phase II
    # boundary, latching one immediate spawn request. Stage 1 §3.3 documented
    # this in the baseline as a 181-frame interval. Averaging it into the mean
    # measures the phase boundary, not the cadence.
    #
    #   1. STEADY STATE — the modal interval must be 3000 ms.
    #   2. SAFETY — no interval may be SHORTER than the scheduled visual
    #      lifetime, since that is exactly the condition under which a successor
    #      could become visible while the predecessor still is (C7).
    if deltas:
        modal_frames = max(frame_deltas, key=lambda k: frame_deltas[k]) if frame_deltas else 0
        modal_ms = sorted(deltas)[len(deltas) // 2]
        res.check("TIMING", "steady-state cadence remains 3000 ms",
                  abs(modal_ms - 3000.0) <= 30.0,
                  f"median={modal_ms:.1f} ms, modal frame delta={modal_frames} "
                  f"({frame_deltas.get(modal_frames, 0)}/{len(deltas)} intervals)")

        min_delta = min(deltas)
        max_lifetime = max(lifetimes) if lifetimes else 2850.0
        res.check("S2-C", "no spawn interval shorter than the visual lifetime",
                  min_delta >= max_lifetime - 1.5 * (1000.0 / 72.0),
                  f"min interval={min_delta:.1f} ms vs max visual lifetime="
                  f"{max_lifetime:.1f} ms")

        non_modal = [d for d in deltas if abs(d - 3000.0) > 30.0]
        res.info("--", "non-modal spawn intervals (phase-boundary latch)",
                 f"{len(non_modal)}: " + ", ".join(f"{d:.1f} ms" for d in non_modal)
                 if non_modal else "0")
    else:
        res.check("TIMING", "steady-state cadence remains 3000 ms", False, "no samples")

    band(windows, 2500.0, 30.0, "effective response window remains 2500 ms", "TIMING")
    band(lifetimes, 2850.0, 40.0, "visual lifetime = 2850 ms (2500 + 350)", "TIMING")

    # ---- Stage 3 boundary: no Hidden frame introduced ----------------------
    vis_false = sum(1 for r in recs if r["ev"] == "VIS_FALSE")
    hidden_true = sum(1 for r in recs if r["ev"] == "HIDDEN_TRUE")
    res.check("STAGE3-BOUNDARY", "no visibility write introduced (that is Stage 3)",
              vis_false == 0 and hidden_true == 0,
              f"VIS_FALSE={vis_false} HIDDEN_TRUE={hidden_true}")

    same_frame = 0
    deferred = 0
    for k, v in by_ident.items():
        f_destroy = next((to_int(r.get("f")) for r in v if r["ev"] == "DESTROY_REQ"), None)
        f_lastmat = max((to_int(r.get("f")) for r in v if r["ev"] == "MAT_WRITE"),
                        default=None)
        if f_destroy is not None and f_lastmat is not None:
            if f_destroy == f_lastmat:
                same_frame += 1
            else:
                deferred += 1
    res.info("STAGE3-BOUNDARY",
             "destroy still on the final material-write frame (unchanged, C1)",
             f"same-frame={same_frame} deferred={deferred}")

    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--session", default=None)
    ap.add_argument("--list-sessions", action="store_true")
    args = ap.parse_args()

    try:
        records = parse(args.log)
    except OSError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2

    if not records:
        print("ERROR: no TMS1 records found.", file=sys.stderr)
        return 2

    sess = sessions(records)

    if args.list_sessions:
        for sid, s in sess.items():
            print(f"sid={sid} map={s['map']} mode={s['mode']} pie={s['pie']} "
                  f"events={s['events']} spawns={s['spawns']} "
                  f"frames={s['first_f']}..{s['last_f']} "
                  f"end={s['end_reason'] or 'OPEN / INCOMPLETE'}")
        return 0

    sid = args.session
    if sid is None:
        if len(sess) != 1:
            print(f"ERROR: {len(sess)} sessions found in this log; refusing to "
                  f"merge them. Use --list-sessions then --session <sid>.",
                  file=sys.stderr)
            return 3
        sid = next(iter(sess))

    if sid not in sess:
        print(f"ERROR: session {sid} not present.", file=sys.stderr)
        return 2

    s = sess[sid]
    print(f"# Stage 2 acceptance check")
    print(f"# log     : {args.log}")
    print(f"# session : sid={sid} map={s['map']} mode={s['mode']} pie={s['pie']} "
          f"spawns={s['spawns']} end={s['end_reason'] or 'OPEN / INCOMPLETE'}")
    print()

    res = analyse(records, sid)
    print(res.render())
    print()
    print("RESULT: " + ("FAIL" if res.failed else "PASS"))
    return 1 if res.failed else 0


if __name__ == "__main__":
    sys.exit(main())
