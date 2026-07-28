#!/usr/bin/env python3
"""parse_tm_trace_v2.py — hardened TranquilMind Stage 1 trace parser.

Supersedes Documentation/Stage1/parse_tm_trace.py, which is preserved unmodified
as part of the Stage 1 evidence chain. This tool fixes two identity defects that
were confirmed against real Stage 1 evidence:

  H1  MIXED SESSIONS. One editor process writes one log across many PIE runs.
      Schema 1 had no session marker, so a whole-log parse silently merged four
      PIE sessions. v2 enumerates sessions and REFUSES to merge them.

  H2  UOBJECT UNIQUEID RECYCLING. auid/miduid are object-table indices reused
      after GC. In the authoritative PIE capture, 40 spawns yielded 39 distinct
      auid and 38 distinct miduid, and one miduid appeared under two auid.
      Schema 1's rule would have called that "MID SHARED ACROSS ACTORS —
      serious". It was recycling: the lifetimes do not overlap. v2 requires
      LIFETIME OVERLAP before reporting sharing, and reports UE_ID_RECYCLED
      otherwise.

IDENTITY MODEL
    schema 2 : canonical actor identity is (sid, spawnseq).
               auid / miduid / actor name are DIAGNOSTIC ONLY.
    schema 1 : no stable identity exists. Identity is LEGACY_INFERRED from auid
               and sessions are LEGACY_INFERRED from world-time resets. Anything
               that cannot be established safely is reported as
               LEGACY_LIMITATION rather than guessed.

USAGE
    parse_tm_trace_v2.py <log> --list-sessions
    parse_tm_trace_v2.py <log> --session <sid> --out <report.md>
    parse_tm_trace_v2.py <log> --out <report.md>      # only if 1 session
"""

import argparse
import re
import sys
from collections import OrderedDict, defaultdict

NOT_OBS = "NOT OBSERVED"
UE_ID_RECYCLED = "UE_ID_RECYCLED"
LEGACY_INFERRED = "LEGACY_INFERRED"
LEGACY_LIMITATION = "LEGACY_LIMITATION"

LEDGER_ORDER = [
    ("SPAWN", "spawn"),
    ("OUTCOME", "outcome resolution"),
    ("EXIT_BEGIN", "exit begin"),
    ("MID_CREATE", "MID create"),
    ("EXIT_70", "exit 70%"),
    ("MAT_WRITE", "final material write"),
    ("VIS_FALSE", "visibility false"),
    ("HIDDEN_TRUE", "HiddenInGame true"),
    ("DELEGATE_COMPLETE", "completion delegate"),
    ("DESTROY_REQ", "Destroy request"),
    ("ENDPLAY", "EndPlay"),
]
LAST_OCCURRENCE = {"MAT_WRITE", "SNAP"}

# Events that belong to a session rather than to one presentation.
SESSION_EVENTS = {"SESSION_BEGIN", "SESSION_END", "NEXT_SPAWN"}


# --------------------------------------------------------------------- parse --

def parse_lines(path):
    """Parse every TMS1 line into a dict. Preserves file order."""
    out = []
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
                    rec.setdefault(k, v)          # first wins; payloads never clobber header
            if "ev" not in rec or "f" not in rec:
                continue
            try:
                rec["_f"] = int(rec["f"])
            except ValueError:
                continue
            try:
                rec["_wt"] = float(rec.get("wt", "-1"))
            except ValueError:
                rec["_wt"] = -1.0
            rec["_schema"] = int(rec.get("schema", "1"))
            out.append(rec)
    return out


def segment_sessions(records):
    """Group records into sessions.

    schema 2 : authoritative — group by the sid field.
    schema 1 : LEGACY_INFERRED — a new world resets World->GetTimeSeconds(), so
               a significant backwards step in wt marks a new session. This is a
               heuristic and is labelled as such everywhere it is used.
    """
    sessions = OrderedDict()
    legacy_idx = 0
    prev_wt = None
    cur_key = None

    for r in records:
        if r["_schema"] >= 2:
            key = r.get("sid", "?")
            r["_sid"] = key
            r["_identity"] = "SCHEMA2"
        else:
            wt = r["_wt"]
            # A drop of >1s in world time means a new world was brought up.
            if prev_wt is None or (wt >= 0 and wt < prev_wt - 1.0):
                legacy_idx += 1
                cur_key = f"L{legacy_idx}"
            if wt >= 0:
                prev_wt = wt
            key = cur_key
            r["_sid"] = key
            r["_identity"] = LEGACY_INFERRED
        sessions.setdefault(key, []).append(r)
    return sessions


def actor_key(rec):
    """Canonical actor key. schema 2 -> (sid, spawnseq); schema 1 -> legacy auid."""
    if rec["_schema"] >= 2:
        ss = rec.get("spawnseq", "-1")
        if ss in ("-1", "", None):
            return None                       # session-level event
        return (rec["_sid"], f"seq{ss}")
    if rec["ev"] in SESSION_EVENTS:
        return None
    return (rec["_sid"], f"auid{rec.get('auid','?')}")


# ----------------------------------------------------------------- lifetimes --

class Lifetime:
    __slots__ = ("key", "first_f", "last_f", "spawn_f", "endplay_f",
                 "auids", "miduids", "events", "complete")

    def __init__(self, key):
        self.key = key
        self.first_f = None
        self.last_f = None
        self.spawn_f = None
        self.endplay_f = None
        self.auids = set()
        self.miduids = set()
        self.events = []
        self.complete = False

    def add(self, rec):
        f = rec["_f"]
        self.first_f = f if self.first_f is None else min(self.first_f, f)
        self.last_f = f if self.last_f is None else max(self.last_f, f)
        if rec["ev"] == "SPAWN":
            self.spawn_f = f
        if rec["ev"] == "ENDPLAY":
            self.endplay_f = f
            self.complete = True
        if rec.get("auid") not in (None, "0"):
            self.auids.add(rec["auid"])
        mu = rec.get("miduid")
        if mu not in (None, "-1", ""):
            self.miduids.add(mu)
        self.events.append(rec)

    @property
    def ambiguous(self):
        """True when this key demonstrably conflates more than one presentation.

        Only reachable on schema 1: keying by auid merges an actor with whatever
        later actor UE handed the same recycled index to. Such a key must be
        excluded from per-lifetime metrics — comparing actor A's DESTROY_REQ
        against actor B's last MAT_WRITE yields nonsense (a -4900 frame
        'separation' was produced this way on the archived PIE capture).
        """
        return sum(1 for e in self.events if e["ev"] == "SPAWN") > 1

    @property
    def lo(self):
        return self.spawn_f if self.spawn_f is not None else self.first_f

    @property
    def hi(self):
        # Open/incomplete lifetime: extend to the last frame actually seen.
        return self.endplay_f if self.endplay_f is not None else self.last_f

    def overlaps(self, other):
        return self.lo <= other.hi and other.lo <= self.hi


def build_lifetimes(records):
    lifetimes = OrderedDict()
    for r in records:
        k = actor_key(r)
        if k is None:
            continue
        lifetimes.setdefault(k, Lifetime(k)).add(r)
    return lifetimes


# ------------------------------------------------------------------ analysis --

def analyse_identity(lifetimes):
    """Lifetime-aware identity rules. Never infers sharing from a repeated UID."""
    findings = []

    def group(attr):
        g = defaultdict(list)
        for lt in lifetimes.values():
            for v in getattr(lt, attr):
                g[v].append(lt)
        return g

    for label, attr, shared_tag in (
        ("MID", "miduids", "SHARED_MID"),
        ("actor UID", "auids", "SHARED_ACTOR_STATE"),
    ):
        recycled, shared = [], []
        for uid, lts in group(attr).items():
            if len(lts) < 2:
                continue
            conc = False
            for i in range(len(lts)):
                for j in range(i + 1, len(lts)):
                    if lts[i].overlaps(lts[j]):
                        conc = True
                        shared.append(
                            f"{uid}: {lts[i].key[1]} [{lts[i].lo}..{lts[i].hi}] "
                            f"CONCURRENT WITH {lts[j].key[1]} [{lts[j].lo}..{lts[j].hi}]")
            if not conc:
                spans = ", ".join(f"{l.key[1]} [{l.lo}..{l.hi}]" for l in lts)
                recycled.append(f"{uid}: {spans}")
        if shared:
            findings.append((f"{label} sharing", f"*** {shared_tag} — SERIOUS ***",
                             "; ".join(shared)))
        elif recycled:
            findings.append((f"{label} sharing", UE_ID_RECYCLED,
                             f"{len(recycled)} {label} value(s) reused across "
                             f"NON-overlapping lifetimes — index recycling after GC, "
                             f"not shared state: " + "; ".join(recycled)))
        else:
            findings.append((f"{label} sharing", "none detected",
                             f"every {label} value belongs to exactly one lifetime"))
    return findings


def analyse_pooling(lifetimes, schema):
    """Pooling requires positive evidence. A recycled UID is NOT evidence."""
    pool_events = [e for lt in lifetimes.values() for e in lt.events
                   if e["ev"] == "DESTROY_REQ" and e.get("mode") == "pool"]
    if pool_events:
        return ("Pooling", "POOL_RETURN EVENTS PRESENT",
                f"{len(pool_events)} DESTROY_REQ with mode=pool")

    if schema >= 2:
        # Reuse of one presentation identity without EndPlay would be pooling.
        reused = [lt for lt in lifetimes.values()
                  if sum(1 for e in lt.events if e["ev"] == "SPAWN") > 1]
        if reused:
            return ("Pooling", "POOL_REUSE — identity respawned without EndPlay",
                    f"{len(reused)} identity(ies) with >1 SPAWN")
        n_end = sum(1 for lt in lifetimes.values() if lt.complete)
        return ("Pooling", "NO — fresh spawn each cycle",
                f"{len(lifetimes)} distinct (sid,spawnseq) identities, "
                f"{n_end} with ENDPLAY, 0 pool-return events. "
                f"Not inferred from UID values.")
    return ("Pooling", f"NO (evidence: no pool-return events) — {LEGACY_LIMITATION}",
            "schema 1 has no stable identity; absence of pooling is asserted only "
            "from the absence of mode=pool events, never from UID uniqueness")


def analyse_exit(lifetimes):
    findings = []
    sep_dm, sep_ed, finals = set(), set(), set()
    vis_false = hidden_true = 0
    ambiguous = [lt for lt in lifetimes.values() if lt.ambiguous]
    for lt in lifetimes.values():
        if lt.ambiguous:
            continue          # conflated key — see Lifetime.ambiguous
        ev = defaultdict(list)
        for e in lt.events:
            ev[e["ev"]].append(e)
        vis_false += len(ev["VIS_FALSE"])
        hidden_true += len(ev["HIDDEN_TRUE"])
        if ev["DESTROY_REQ"] and ev["MAT_WRITE"]:
            sep_dm.add(ev["DESTROY_REQ"][0]["_f"] - ev["MAT_WRITE"][-1]["_f"])
        if ev["ENDPLAY"] and ev["DESTROY_REQ"]:
            sep_ed.add(ev["ENDPLAY"][0]["_f"] - ev["DESTROY_REQ"][0]["_f"])
        for e in ev["MAT_WRITE"][-3:]:
            finals.add(e.get("value"))
    findings.append(("Teardown separation",
                     f"DESTROY_REQ-lastMAT_WRITE={sorted(sep_dm)}  "
                     f"ENDPLAY-DESTROY_REQ={sorted(sep_ed)}",
                     "Spec §11 check 3 requires >=1 full frame"))
    findings.append(("Hidden phase",
                     f"VIS_FALSE={vis_false}  HIDDEN_TRUE={hidden_true}",
                     "zero of both means no Hidden phase exists"))
    findings.append(("Fade terminal value", f"final MAT_WRITE values={sorted(finals)}",
                     "0.000000 means the fade completes; >0 means truncation"))
    if ambiguous:
        findings.append((
            "Excluded from exit metrics",
            f"{LEGACY_LIMITATION} — {len(ambiguous)} conflated identity key(s)",
            "schema-1 auid recycling merged two presentations under one key; "
            "those keys are excluded rather than producing a meaningless "
            "cross-actor frame difference: "
            + "; ".join(f"{lt.key[1]} [{lt.lo}..{lt.hi}]" for lt in ambiguous)))
    return findings


def analyse_overlap(records):
    def ints(key):
        out = []
        for r in records:
            v = r.get(key, "")
            if v.lstrip("-").isdigit():
                out.append((r["_f"], int(v)))
        return out

    vp, at = ints("visiblepresentations"), ints("activetargets")
    max_vp = max((v for _, v in vp), default=-1)
    max_at = max((v for _, v in at), default=-1)
    frames = sorted({f for f, v in vp if v >= 2})
    runs = []
    for f in frames:
        if runs and f == runs[-1][-1] + 1:
            runs[-1].append(f)
        else:
            runs.append([f])
    detail = (f"max visiblepresentations={max_vp}, max activetargets={max_at}, "
              f"{len(frames)} frames >=2 in {len(runs)} run(s)"
              + (f", longest={max(len(r) for r in runs)} frames" if runs else ""))
    if max_at >= 2:
        return ("Overlap", "*** ESCALATE — activetargets>=2 (scoring concurrency) ***", detail)
    if max_vp >= 2:
        return ("Overlap", "CONFIRMED — presentation-ownership defect "
                           "(visiblepresentations>=2 while activetargets<=1)", detail)
    return ("Overlap", "not observed", detail)


def analyse_approach(lifetimes):
    moved_actor = moved_vm = static_actor = 0
    n_ambig = sum(1 for lt in lifetimes.values() if lt.ambiguous)
    for lt in lifetimes.values():
        if lt.ambiguous:
            continue          # conflated key — see Lifetime.ambiguous
        snaps = [e for e in lt.events if e["ev"] == "SNAP"]
        eb = [e for e in lt.events if e["ev"] == "EXIT_BEGIN"]
        if not snaps or not eb:
            continue
        after = [s for s in snaps if s["_f"] >= eb[0]["_f"]]
        if len(after) < 2:
            continue
        if after[0].get("axform") != after[-1].get("axform"):
            moved_actor += 1
        else:
            static_actor += 1
        if after[0].get("vmrel") != after[-1].get("vmrel"):
            moved_vm += 1
    return ("Approach during exit",
            ("ACTOR TRANSFORM FROZEN" if moved_actor == 0 and static_actor
             else f"actor transform changed in {moved_actor} exit(s)"),
            f"actor static in {static_actor} exit(s), changed in {moved_actor}; "
            f"VisualMotion relative transform changed in {moved_vm}"
            + (f"; {n_ambig} conflated key(s) excluded ({LEGACY_LIMITATION})"
               if n_ambig else ""))


# -------------------------------------------------------------------- report --

def md_table(headers, rows):
    out = ["| " + " | ".join(headers) + " |",
           "|" + "|".join(["---"] * len(headers)) + "|"]
    for r in rows:
        out.append("| " + " | ".join(str(c) for c in r) + " |")
    return "\n".join(out)


def session_summary(sessions):
    rows = []
    for sid, recs in sessions.items():
        begin = next((r for r in recs if r["ev"] == "SESSION_BEGIN"), None)
        end = next((r for r in recs if r["ev"] == "SESSION_END"), None)
        lts = build_lifetimes(recs)
        schema = max(r["_schema"] for r in recs)
        rows.append([
            sid,
            f"schema {schema}" + ("" if schema >= 2 else f" ({LEGACY_INFERRED})"),
            begin.get("map", "?") if begin else "?",
            begin.get("mode", "?") if begin else "?",
            ("PIE" if begin and begin.get("pie") == "1" else
             "game/other" if begin else "?"),
            len(recs),
            len(lts),
            f"{recs[0]['_f']}..{recs[-1]['_f']}",
            (end.get("reasontext", end.get("reason", "?")) if end else "open/none"),
        ])
    return md_table(["sid", "schema", "map", "mode", "worldtype",
                     "events", "actors", "frame range", "end reason"], rows)


def build_report(path, sid, recs, sessions):
    schema = max(r["_schema"] for r in recs)
    lifetimes = build_lifetimes(recs)
    L = [f"# Stage 1 trace report (v2) — session `{sid}`", "",
         f"Source log: `{path}`",
         f"Schema: **{schema}**"
         + ("" if schema >= 2 else f"  — identity is **{LEGACY_INFERRED}**"),
         f"Sessions in file: {len(sessions)} (this report covers **one**)",
         f"Events in session: {len(recs)}   Actor identities: {len(lifetimes)}", ""]

    if schema < 2:
        L += ["> **LEGACY NOTICE.** This is a schema 1 log. It has no stable actor",
              "> identity and no session marker. Actor identity is inferred from",
              "> `auid`, which UE recycles after GC, and session boundaries are",
              "> inferred from world-time resets. Identity conclusions are",
              f"> `{LEGACY_INFERRED}`; anything not safely recoverable is reported",
              f"> as `{LEGACY_LIMITATION}`. `auid`/`miduid` are NEVER claimed to be",
              "> globally unique.", ""]

    L += ["## Sessions in this file", "", session_summary(sessions), ""]

    L += ["## Actor lifetimes", ""]
    rows = []
    for k, lt in lifetimes.items():
        rows.append([f"{k[0]}/{k[1]}", lt.lo, lt.hi,
                     "complete" if lt.complete else "**OPEN / INCOMPLETE**",
                     ",".join(sorted(lt.auids)) or "-",
                     ",".join(sorted(lt.miduids)) or "-"])
    L += [md_table(["identity", "first f", "last f", "lifetime",
                    "auid (diagnostic)", "miduid (diagnostic)"], rows[:60]), ""]
    if len(rows) > 60:
        L += [f"*(showing 60 of {len(rows)} identities)*", ""]

    L += ["## Findings", ""]
    f = []
    f += analyse_identity(lifetimes)
    f.append(analyse_pooling(lifetimes, schema))
    f.append(analyse_overlap(recs))
    f.append(analyse_approach(lifetimes))
    f += analyse_exit(lifetimes)
    L += [md_table(["Question", "Finding", "Evidence"], f), ""]

    L += ["## Not determined by this tool", "",
          "- Whether a terminal flash is visible. That is a rendered-viewport",
          "  observation and no log field can establish it.", ""]
    return "\n".join(L)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logfile")
    ap.add_argument("--list-sessions", action="store_true")
    ap.add_argument("--session")
    ap.add_argument("--out")
    args = ap.parse_args()

    records = parse_lines(args.logfile)
    if not records:
        print("No TMS1 lines found.", file=sys.stderr)
        sys.exit(2)

    sessions = segment_sessions(records)

    if args.list_sessions:
        print(session_summary(sessions))
        sys.exit(0)

    if args.session:
        if args.session not in sessions:
            print(f"No such session '{args.session}'. Available: "
                  f"{', '.join(sessions)}", file=sys.stderr)
            sys.exit(2)
        sid = args.session
    elif len(sessions) == 1:
        sid = next(iter(sessions))
    else:
        # FAIL CLOSED. Never silently merge sessions — this is defect H1.
        print(f"ERROR: {len(sessions)} sessions found in this log; refusing to "
              f"merge them.\nSelect one with --session <sid>, or inspect with "
              f"--list-sessions.\nSessions: {', '.join(sessions)}", file=sys.stderr)
        sys.exit(3)

    report = build_report(args.logfile, sid, sessions[sid], sessions)
    if args.out:
        with open(args.out, "w", encoding="utf-8") as fh:
            fh.write(report)
        print(f"Wrote {args.out}  (session {sid}, {len(sessions[sid])} events)")
    else:
        print(report)


if __name__ == "__main__":
    main()
