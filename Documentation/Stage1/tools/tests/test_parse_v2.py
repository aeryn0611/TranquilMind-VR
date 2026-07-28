#!/usr/bin/env python3
"""Regression fixtures for parse_tm_trace_v2.py — cases A..J.

Each fixture is a synthetic TMS1 log exercising one identity hazard. The suite
proves four properties the schema 1 tooling did not have:

  * sessions are never silently merged
  * overlap never relabels the predecessor
  * recycled UIDs are never classified as shared state
  * genuine CONCURRENT sharing is still detected as serious

Run:  python3 test_parse_v2.py
"""

import io
import os
import sys
import contextlib

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import parse_tm_trace_v2 as P   # noqa: E402

FIX = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures")
os.makedirs(FIX, exist_ok=True)

RESULTS = []


def h2(f, wt, ev, sid, seq, auid, cyc=1, extra=""):
    return (f"[log]TMS1|schema=2|f={f}|rt={wt:.6f}|wt={wt:.6f}|ev={ev}|sid={sid}"
            f"|spawnseq={seq}|aname=A_{auid}|auid={auid}|cyc={cyc}{extra}")


def h1(f, wt, ev, auid, cyc=1, extra=""):
    return (f"[log]TMS1|f={f}|rt={wt:.6f}|wt={wt:.6f}|ev={ev}|aname=A_{auid}"
            f"|auid={auid}|cyc={cyc}{extra}")


def write(name, lines):
    p = os.path.join(FIX, name)
    with open(p, "w") as fh:
        fh.write("\n".join(lines) + "\n")
    return p


def life2(sid, seq, auid, f0, f1, mid=None, endplay=True, cyc=1):
    """A schema-2 actor lifetime SPAWN..ENDPLAY with optional MID + snapshots."""
    out = [h2(f0, f0 / 72.0, "SPAWN", sid, seq, auid, cyc)]
    if mid is not None:
        out.append(h2(f0 + 1, (f0 + 1) / 72.0, "MID_CREATE", sid, seq, auid, cyc,
                      f"|miduid={mid}|caller=test"))
        out.append(h2(f0 + 2, (f0 + 2) / 72.0, "SNAP", sid, seq, auid, cyc,
                      f"|miduid={mid}|midvalid=1|activetargets=1|visiblepresentations=1"))
    if endplay:
        out.append(h2(f1, f1 / 72.0, "ENDPLAY", sid, seq, auid, cyc, "|reason=0"))
    return out


def sess(sid, mapname, mode, f0, pie=1):
    return (f"[log]TMS1|schema=2|f={f0}|rt=0.0|wt=0.0|ev=SESSION_BEGIN|sid={sid}"
            f"|spawnseq=-1|aname=session|auid=0|cyc=0|map={mapname}"
            f"|worldtype=3|pie={pie}|mode={mode}|netmode=0")


def run(path, session=None):
    recs = P.parse_lines(path)
    sessions = P.segment_sessions(recs)
    sid = session or (next(iter(sessions)) if len(sessions) == 1 else None)
    if sid is None:
        return sessions, None, None
    lts = P.build_lifetimes(sessions[sid])
    return sessions, sid, lts


def check(name, cond, detail=""):
    RESULTS.append((name, bool(cond), detail))
    print(f"  {'PASS' if cond else 'FAIL'}  {name}" + (f"   [{detail}]" if detail and not cond else ""))


# ---------------------------------------------------------------- A ----------
print("\nA. one session, distinct actor and MID identities")
p = write("A_single.log", [sess(1, "L_Env", "Demo", 100)]
          + life2(1, 0, 500, 101, 130, mid=900)
          + life2(1, 1, 501, 131, 160, mid=901))
sessions, sid, lts = run(p)
check("A: exactly 1 session", len(sessions) == 1, str(list(sessions)))
check("A: 2 actor identities", len(lts) == 2, str(len(lts)))
ident = dict((k, (v, d)) for k, v, d in P.analyse_identity(lts))
check("A: no MID sharing", ident["MID sharing"][0] == "none detected", ident["MID sharing"][0])

# ---------------------------------------------------------------- B ----------
print("\nB. multiple PIE sessions in one editor log")
p = write("B_multi.log",
          [sess(1, "L_Env", "Demo", 100)] + life2(1, 0, 500, 101, 130, mid=900)
          + [sess(2, "L_Void", "Demo", 200)] + life2(2, 0, 500, 201, 230, mid=900))
sessions, sid, lts = run(p)
check("B: 2 sessions detected", len(sessions) == 2, str(list(sessions)))
# Must FAIL CLOSED when no session selected.
rc = None
try:
    argv = sys.argv
    sys.argv = ["x", p, "--out", os.path.join(FIX, "B_out.md")]
    with contextlib.redirect_stderr(io.StringIO()):
        P.main()
except SystemExit as e:
    rc = e.code
finally:
    sys.argv = argv
check("B: fails closed without --session (exit 3)", rc == 3, f"rc={rc}")
_, _, lts1 = run(p, session="1")
_, _, lts2 = run(p, session="2")
check("B: each session isolated to 1 actor", len(lts1) == 1 and len(lts2) == 1,
      f"{len(lts1)},{len(lts2)}")
check("B: identical auid+miduid across sessions NOT merged",
      set(lts1) != set(lts2))

# ---------------------------------------------------------------- C ----------
print("\nC. overlap — actor n still emitting after actor n+1 spawns")
lines = [sess(1, "L_Env", "Demo", 100)]
lines += [h2(101, 101 / 72, "SPAWN", 1, 0, 500, 1)]
lines += [h2(120, 120 / 72, "SPAWN", 1, 1, 501, 2)]          # successor spawns
lines += [h2(125, 125 / 72, "MAT_WRITE", 1, 0, 500, 1, "|param=EdgeOpacity|value=0.5")]
lines += [h2(126, 126 / 72, "SNAP", 1, 0, 500, 1,
             "|midvalid=1|miduid=900|activetargets=1|visiblepresentations=2")]
lines += [h2(130, 130 / 72, "ENDPLAY", 1, 0, 500, 1, "|reason=0")]
lines += [h2(160, 160 / 72, "ENDPLAY", 1, 1, 501, 2, "|reason=0")]
p = write("C_overlap.log", lines)
sessions, sid, lts = run(p)
pred = lts[("1", "seq0")]
check("C: predecessor keeps its own identity after successor spawns",
      pred.spawn_f == 101 and pred.endplay_f == 130, f"{pred.spawn_f}..{pred.endplay_f}")
check("C: predecessor's post-successor events not relabelled",
      all(e.get("spawnseq") == "0" for e in pred.events))
ov = P.analyse_overlap(sessions[sid])
check("C: overlap reported as presentation-ownership defect",
      "CONFIRMED" in ov[1] and "ESCALATE" not in ov[1], ov[1])

# ---------------------------------------------------------------- D ----------
print("\nD. auid recycled after the earlier actor ended")
p = write("D_auid_recycle.log", [sess(1, "L_Env", "Demo", 100)]
          + life2(1, 0, 500, 101, 130)
          + life2(1, 1, 500, 200, 230))          # same auid, later, non-overlapping
sessions, sid, lts = run(p)
check("D: 2 distinct identities despite identical auid", len(lts) == 2, str(len(lts)))
ident = dict((k, (v, d)) for k, v, d in P.analyse_identity(lts))
check("D: reported UE_ID_RECYCLED, not SHARED_ACTOR_STATE",
      ident["actor UID sharing"][0] == P.UE_ID_RECYCLED, ident["actor UID sharing"][0])

# ---------------------------------------------------------------- E ----------
print("\nE. miduid recycled after the earlier actor ended")
p = write("E_mid_recycle.log", [sess(1, "L_Env", "Demo", 100)]
          + life2(1, 0, 500, 101, 130, mid=900)
          + life2(1, 1, 501, 200, 230, mid=900))  # same MID, non-overlapping
sessions, sid, lts = run(p)
ident = dict((k, (v, d)) for k, v, d in P.analyse_identity(lts))
check("E: reported UE_ID_RECYCLED, not SHARED_MID",
      ident["MID sharing"][0] == P.UE_ID_RECYCLED, ident["MID sharing"][0])

# ---------------------------------------------------------------- F ----------
print("\nF. genuine CONCURRENT MID sharing — must be serious")
lines = [sess(1, "L_Env", "Demo", 100)]
lines += life2(1, 0, 500, 101, 200, mid=900)      # 101..200
lines += life2(1, 1, 501, 150, 250, mid=900)      # 150..250  -> overlaps
p = write("F_shared_mid.log", lines)
sessions, sid, lts = run(p)
ident = dict((k, (v, d)) for k, v, d in P.analyse_identity(lts))
check("F: concurrent MID sharing detected as SERIOUS",
      "SHARED_MID" in ident["MID sharing"][0] and "SERIOUS" in ident["MID sharing"][0],
      ident["MID sharing"][0])

# ---------------------------------------------------------------- G ----------
print("\nG. missing EndPlay / incomplete lifetime")
p = write("G_open.log", [sess(1, "L_Env", "Demo", 100)]
          + life2(1, 0, 500, 101, 130, mid=900, endplay=False))
sessions, sid, lts = run(p)
lt = lts[("1", "seq0")]
check("G: lifetime marked incomplete", lt.complete is False)
check("G: open lifetime still bounded by last seen frame", lt.hi is not None)
pool = P.analyse_pooling(lts, 2)
check("G: incomplete lifetime alone does NOT imply pooling",
      "POOL" not in pool[1].upper() or "NO" in pool[1], pool[1])

# ---------------------------------------------------------------- H ----------
print("\nH. forced teardown cycle")
lines = [sess(1, "L_Env", "Demo", 100)] + life2(1, 0, 500, 101, 130, mid=900)
lines += [h2(131, 131 / 72, "OUTCOME", 1, 0, 500, 1, "|outcome=Void|rt_ms=0.0")]
lines += [f"[log]TMS1|schema=2|f=132|rt=1.8|wt=1.8|ev=SESSION_END|sid=1|spawnseq=-1"
          f"|aname=session|auid=0|cyc=0|reason=2|reasontext=EndPlayInEditor|spawns=1"]
p = write("H_teardown.log", lines)
sessions, sid, lts = run(p)
end = [r for r in sessions[sid] if r["ev"] == "SESSION_END"][0]
check("H: SESSION_END carries teardown reason", end.get("reasontext") == "EndPlayInEditor",
      end.get("reasontext", "?"))
check("H: forced teardown does not corrupt identity", len(lts) == 1, str(len(lts)))

# ---------------------------------------------------------------- I ----------
print("\nI. schema 1 archived log")
lines = [h1(101, 1.40, "SPAWN", 500), h1(130, 1.80, "ENDPLAY", 500, extra="|reason=0"),
         h1(201, 2.79, "SPAWN", 501), h1(230, 3.19, "ENDPLAY", 501, extra="|reason=0")]
p = write("I_schema1.log", lines)
sessions, sid, lts = run(p)
check("I: schema 1 parsed", len(sessions) == 1 and len(lts) == 2,
      f"{len(sessions)},{len(lts)}")
check("I: identity labelled LEGACY_INFERRED",
      all(r["_identity"] == P.LEGACY_INFERRED for r in sessions[sid]))
rep = P.build_report(p, sid, sessions[sid], sessions)
check("I: report carries the LEGACY notice", "LEGACY NOTICE" in rep)
check("I: schema 1 pooling answer carries LEGACY_LIMITATION",
      P.LEGACY_LIMITATION in P.analyse_pooling(lts, 1)[1])
# schema-1 world-time reset must split sessions rather than merge them
lines2 = lines + [h1(300, 0.10, "SPAWN", 500), h1(330, 0.50, "ENDPLAY", 500, extra="|reason=0")]
p2 = write("I_schema1_two_worlds.log", lines2)
s2, _, _ = run(p2)
check("I: schema-1 world-time reset splits sessions (not merged)", len(s2) == 2, str(list(s2)))

# ---------------------------------------------------------------- J ----------
print("\nJ. schema 2 log")
p = write("J_schema2.log", [sess(7, "UEDPIE_0_L_Env", "Demo", 100)]
          + life2(7, 0, 500, 101, 130, mid=900))
sessions, sid, lts = run(p)
check("J: sid taken from the log, not inferred", sid == "7", sid)
check("J: identity is SCHEMA2", all(r["_identity"] == "SCHEMA2" for r in sessions[sid]))
check("J: canonical key is (sid,spawnseq)", ("7", "seq0") in lts, str(list(lts)))

# ------------------------------------------------------------- summary -------
n_pass = sum(1 for _, ok, _ in RESULTS if ok)
n_fail = len(RESULTS) - n_pass
print(f"\n{'='*60}\nRESULT: {n_pass}/{len(RESULTS)} checks passed, {n_fail} failed\n{'='*60}")
sys.exit(1 if n_fail else 0)
