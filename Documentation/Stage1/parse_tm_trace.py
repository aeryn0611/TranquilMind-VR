#!/usr/bin/env python3
"""
parse_tm_trace.py — TranquilMind Stage 1 trace parser.

Turns a UE log containing TMS1 lines into the tables required by
TranquilMind_Implementation_Spec.md §11 and Stage1_Instrumentation_Kit.md §7.

It reports ONLY what the log contains. Where the log is silent it prints
NOT OBSERVED. It never infers a finding the evidence does not support.

Usage:
    python3 parse_tm_trace.py <logfile> [--out stage1_report_tables.md]
"""

import argparse
import re
import sys
from collections import OrderedDict, defaultdict

LEDGER_ORDER = [
    ("SPAWN",             "spawn"),
    ("OUTCOME",           "outcome resolution"),
    ("EXIT_BEGIN",        "exit begin"),
    ("EXIT_70",           "exit 70%"),
    ("MAT_WRITE",         "final material write"),   # last occurrence
    ("VIS_FALSE",         "visibility false"),
    ("HIDDEN_TRUE",       "HiddenInGame true"),
    ("DELEGATE_COMPLETE", "completion delegate"),
    ("DESTROY_REQ",       "Destroy request"),
    ("ENDPLAY",           "EndPlay"),
    ("NEXT_SPAWN",        "next target spawn"),
]
LAST_OCCURRENCE = {"MAT_WRITE", "SNAP"}
NOT_OBS = "NOT OBSERVED"


def parse(path):
    events = []
    pat = re.compile(r"TMS1\|(.*)$")
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            m = pat.search(raw.rstrip("\n"))
            if not m:
                continue
            rec = OrderedDict()
            for part in m.group(1).split("|"):
                if "=" in part:
                    k, v = part.split("=", 1)
                    rec[k] = v
            if "ev" in rec and "f" in rec:
                try:
                    rec["_f"] = int(rec["f"])
                except ValueError:
                    continue
                rec["_cyc"] = int(rec.get("cyc", -1))
                events.append(rec)
    return events


def num(rec, key, default=None):
    try:
        return float(rec[key])
    except (KeyError, ValueError, TypeError):
        return default


def by_cycle(events):
    out = defaultdict(list)
    for e in events:
        out[e["_cyc"]].append(e)
    return OrderedDict(sorted(out.items()))


def ledger(cyc_events):
    picks = {}
    for ev_name, _ in LEDGER_ORDER:
        hits = [e for e in cyc_events if e["ev"] == ev_name]
        if not hits:
            continue
        picks[ev_name] = hits[-1] if ev_name in LAST_OCCURRENCE else hits[0]
    spawn_f = picks["SPAWN"]["_f"] if "SPAWN" in picks else None
    rows = []
    for ev_name, label in LEDGER_ORDER:
        e = picks.get(ev_name)
        if not e:
            rows.append((label, NOT_OBS, NOT_OBS))
            continue
        delta = str(e["_f"] - spawn_f) if spawn_f is not None else NOT_OBS
        rows.append((label, str(e["_f"]), delta))
    return rows, picks


def md_table(headers, rows):
    out = ["| " + " | ".join(headers) + " |",
           "|" + "|".join(["---"] * len(headers)) + "|"]
    for r in rows:
        out.append("| " + " | ".join(str(c) for c in r) + " |")
    return "\n".join(out)


def snap_scalars(rec):
    return {k[3:]: v for k, v in rec.items() if k.startswith("sc:")}


def analyse(events, cycles):
    """Apply Kit §6 decision rules. Emit only evidence-supported findings."""
    f = []

    # ---- 6.4 pooling -------------------------------------------------------
    uids = [c[0]["auid"] for c in
            [[e for e in evs if e["ev"] == "SPAWN"] for evs in cycles.values()] if c]
    endplays = [e for evs in cycles.values() for e in evs if e["ev"] == "ENDPLAY"]
    if not uids:
        f.append(("4. Pooling exists", NOT_OBS, "no SPAWN events found"))
    elif len(set(uids)) < len(uids):
        f.append(("4. Pooling exists", "YES — actor reused",
                  f"auid repeats across cycles: {uids}"))
    elif endplays:
        f.append(("4. Pooling exists", "NO — fresh spawn each cycle",
                  f"auid unique per cycle {uids}; {len(endplays)} ENDPLAY event(s)"))
    else:
        f.append(("4. Pooling exists", "INCONCLUSIVE",
                  f"auid unique {uids} but no ENDPLAY observed"))

    # ---- 6.5 MID / motion reuse -------------------------------------------
    mid_to_actors = defaultdict(set)
    for e in events:
        if e["ev"] in ("SNAP", "SPAWN", "MID_CREATE"):
            mu = e.get("miduid")
            if mu and mu != "-1":
                mid_to_actors[mu].add(e.get("auid"))
    shared = {m: a for m, a in mid_to_actors.items() if len(a) > 1}
    if shared:
        f.append(("5. MID / motion state reuse", "MID SHARED ACROSS ACTORS — serious",
                  f"miduid seen under multiple auid: {shared}"))
    elif mid_to_actors:
        f.append(("5. MID / motion state reuse", "no MID sharing detected",
                  f"{len(mid_to_actors)} distinct miduid, each under one auid"))
    else:
        f.append(("5. MID / motion state reuse", NOT_OBS, "no miduid recorded"))

    # ---- 6.1 flash ---------------------------------------------------------
    sigs = []
    for cyc, evs in cycles.items():
        mw = [e for e in evs if e["ev"] == "MAT_WRITE"]
        vf = [e for e in evs if e["ev"] == "VIS_FALSE"]
        ht = [e for e in evs if e["ev"] == "HIDDEN_TRUE"]
        dr = [e for e in evs if e["ev"] == "DESTROY_REQ"]
        ep = [e for e in evs if e["ev"] == "ENDPLAY"]
        mc = [e for e in evs if e["ev"] == "MID_CREATE"]
        sn = [e for e in evs if e["ev"] == "SNAP"]

        if mw and vf and mw[-1]["_f"] > vf[0]["_f"]:
            sigs.append(f"cyc{cyc}: ORDERING — last MAT_WRITE f={mw[-1]['_f']} after VIS_FALSE f={vf[0]['_f']}")
        if vf and ht and vf[0]["_f"] != ht[0]["_f"]:
            sigs.append(f"cyc{cyc}: VIS_FALSE f={vf[0]['_f']} and HIDDEN_TRUE f={ht[0]['_f']} on different frames")
        if dr and mw and dr[0]["_f"] == mw[-1]["_f"]:
            sigs.append(f"cyc{cyc}: SAME-FRAME DESTROY — DESTROY_REQ and final MAT_WRITE both f={dr[0]['_f']}")
        if ep and vf and (ep[0]["_f"] - vf[0]["_f"]) == 0:
            sigs.append(f"cyc{cyc}: NO DEFERRED DESTROY — ENDPLAY == VIS_FALSE f={ep[0]['_f']}")
        if mc:
            sigs.append(f"cyc{cyc}: MID_CREATE called {len(mc)} time(s) — callers {[e.get('caller') for e in mc]}")
        for s in sn:
            if s.get("midvalid") == "0":
                sigs.append(f"cyc{cyc}: MID LOST mid-window at f={s['_f']}")
                break
        if sn:
            last = sn[-1]
            for pname, pval in snap_scalars(last).items():
                try:
                    if pname.lower().startswith("opac") and float(pval) > 0.001:
                        sigs.append(f"cyc{cyc}: FADE TRUNCATED — final {pname}={pval} at f={last['_f']}")
                except ValueError:
                    pass
    f.append(("1. Terminal flash cause",
              "; ".join(sorted(set(sigs))) if sigs else
              "no matching signature — apply Kit §6.1 manually",
              "see Kit §6.1 rows"))

    # ---- 6.2 continued approach -------------------------------------------
    approach = []
    for cyc, evs in cycles.items():
        sn = [e for e in evs if e["ev"] == "SNAP"]
        eb = [e for e in evs if e["ev"] == "EXIT_BEGIN"]
        if not sn or not eb:
            continue
        after = [s for s in sn if s["_f"] >= eb[0]["_f"]]
        if len(after) < 2:
            continue
        d0, d1 = num(after[0], "apdist"), num(after[-1], "apdist")
        if d0 is None or d1 is None:
            continue
        moved_actor = after[0].get("axform") != after[-1].get("axform")
        moved_vm    = after[0].get("vmrel")  != after[-1].get("vmrel")
        moved_mesh  = after[0].get("mxform") != after[-1].get("mxform")
        if d1 < d0 - 0.5:
            path = ("ACTOR-LEVEL" if moved_actor else
                    "VISUALMOTION RELATIVE" if moved_vm else
                    "MESH-COMPONENT-LEVEL (not in C1-C7 catalogue)" if moved_mesh else
                    "distance fell with no transform change — investigate")
            approach.append(f"cyc{cyc}: apdist {d0:.1f}->{d1:.1f}cm, path={path}")
        else:
            approach.append(f"cyc{cyc}: apdist {d0:.1f}->{d1:.1f}cm — no depth decrease; "
                            f"check scale/rim scalars for apparent looming")
    f.append(("2. Continued-approach transform path",
              "; ".join(approach) if approach else NOT_OBS, "Kit §6.2"))

    # ---- 6.3 overlap -------------------------------------------------------
    max_vis = -1
    max_act = -1
    frames_over_1 = 0
    for e in events:
        v = num(e, "visiblepresentations")
        a = num(e, "activetargets")
        if v is not None and v > max_vis:
            max_vis = v
        if v is not None and v > 1:
            frames_over_1 += 1
        if a is not None and a > max_act:
            max_act = a
    ov = []
    if max_vis >= 2:
        ov.append(f"CONFIRMED OVERLAP — max visiblepresentations={int(max_vis)} "
                  f"over {frames_over_1} snapshot frame(s)")
    elif max_vis >= 0:
        ov.append(f"no overlap observed — max visiblepresentations={int(max_vis)}")
    else:
        ov.append("visiblepresentations not recorded — TargetPresentationClass() unset")
    for cyc, evs in cycles.items():
        ns = [e for e in evs if e["ev"] == "NEXT_SPAWN"]
        ep = [e for e in evs if e["ev"] == "ENDPLAY"]
        if ns and ep and ns[0]["_f"] < ep[0]["_f"]:
            ov.append(f"cyc{cyc}: LIFETIME EXCEEDS CADENCE — NEXT_SPAWN f={ns[0]['_f']} "
                      f"before ENDPLAY f={ep[0]['_f']}")
    if max_act >= 2:
        ov.append(f"*** ESCALATE: activetargets reached {int(max_act)} — "
                  f"two actors registered as targets simultaneously ***")
    f.append(("3. Overlap cause", "; ".join(ov), "Kit §6.3"))

    return f


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logfile")
    ap.add_argument("--out", default="stage1_report_tables.md")
    args = ap.parse_args()

    events = parse(args.logfile)
    if not events:
        print("No TMS1 lines found. Check the log path and that "
              "LogTMStage1 verbosity is at least Log.", file=sys.stderr)
        sys.exit(2)

    cycles = by_cycle(events)
    complete = [c for c, evs in cycles.items()
                if any(e["ev"] == "SPAWN" for e in evs)
                and any(e["ev"] == "EXIT_BEGIN" for e in evs)]

    L = ["# Stage 1 Report Tables (generated)", "",
         f"Source log: `{args.logfile}`",
         f"TMS1 events parsed: {len(events)}",
         f"Cycles seen: {len(cycles)}  |  cycles with SPAWN+EXIT_BEGIN: {len(complete)}", ""]
    if len(complete) < 3:
        L += ["> **WARNING: fewer than 3 complete cycles. Spec §11 requires at least 3 "
              "natural-expiry cycles. Do not write findings from this capture.**", ""]

    L += ["## Table A — Event ledger", ""]
    for cyc, evs in cycles.items():
        rows, _ = ledger(evs)
        L += [f"### Cycle {cyc}", "", md_table(["Event", "frame", "Δ from spawn"], rows), ""]

    L += ["## Table B — Identity ledger", ""]
    idrows = []
    for cyc, evs in cycles.items():
        sp = next((e for e in evs if e["ev"] == "SPAWN"), None)
        mc = [e for e in evs if e["ev"] == "MID_CREATE"]
        idrows.append([
            cyc,
            sp["auid"] if sp else NOT_OBS,
            sp.get("miduid", NOT_OBS) if sp else NOT_OBS,
            sp.get("matiface", NOT_OBS) if sp else NOT_OBS,
            "YES" if mc else "no",
            ", ".join(e.get("caller", "?") for e in mc) or "-",
        ])
    L += [md_table(["cycle", "auid", "miduid @spawn", "matiface",
                    "CreateDynamicMaterialInstance called again?", "caller(s)"], idrows), ""]

    L += ["## Table C — Exit-window snapshots", ""]
    for cyc, evs in cycles.items():
        sn = [e for e in evs if e["ev"] == "SNAP"]
        if not sn:
            L += [f"### Cycle {cyc}", "", NOT_OBS, ""]
            continue
        scal = sorted({k for s in sn for k in snap_scalars(s)})
        hdr = (["frame", "apdist", "apdist_proj"] + [f"sc:{p}" for p in scal] +
               ["midvalid", "miduid", "meshvis", "meshhidden", "actorhidden",
                "destroying", "activeTgts", "visiblePres"])
        rows = []
        for s in sn:
            sc = snap_scalars(s)
            rows.append([s["_f"], s.get("apdist", "-"), s.get("apdist_proj", "-")] +
                        [sc.get(p, "-") for p in scal] +
                        [s.get("midvalid", "-"), s.get("miduid", "-"),
                         s.get("meshvis", "-"), s.get("meshhidden", "-"),
                         s.get("actorhidden", "-"), s.get("beingdestroyed", "-"),
                         s.get("activetargets", "-"), s.get("visiblepresentations", "-")])
        L += [f"### Cycle {cyc}", "", md_table(hdr, rows), ""]

    L += ["## Table D — Findings from decision rules (Kit §6)", "",
          "Each finding cites the evidence that produced it. "
          "`NOT OBSERVED` means the log is silent — leave it silent.", ""]
    L += [md_table(["Question", "Finding", "Rule"], analyse(events, cycles)), ""]
    L += ["## Still required by hand", "",
          "- Research JSONL byte-identical: **YES / NO**",
          "- Flash visible on which cycles (visual observation, not in the log)",
          "- Prerequisite slots P1–P12",
          "- Dispositions for C1–C4 — only where instrumentation proves them; "
          "`NOT YET PROVEN` is a valid answer", ""]

    with open(args.out, "w", encoding="utf-8") as fh:
        fh.write("\n".join(L))
    print(f"Wrote {args.out}  ({len(events)} events, {len(cycles)} cycles, "
          f"{len(complete)} complete)")


if __name__ == "__main__":
    main()
