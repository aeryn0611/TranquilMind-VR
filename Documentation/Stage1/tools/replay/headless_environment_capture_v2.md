# Stage 1 trace report (v2) — session `L1`

Source log: `Documentation/Stage1/evidence/headless_environment_capture.log`
Schema: **1**  — identity is **LEGACY_INFERRED**
Sessions in file: 1 (this report covers **one**)
Events in session: 12785   Actor identities: 40

> **LEGACY NOTICE.** This is a schema 1 log. It has no stable actor
> identity and no session marker. Actor identity is inferred from
> `auid`, which UE recycles after GC, and session boundaries are
> inferred from world-time resets. Identity conclusions are
> `LEGACY_INFERRED`; anything not safely recoverable is reported
> as `LEGACY_LIMITATION`. `auid`/`miduid` are NEVER claimed to be
> globally unique.

## Sessions in this file

| sid | schema | map | mode | worldtype | events | actors | frame range | end reason |
|---|---|---|---|---|---|---|---|---|
| L1 | schema 1 (LEGACY_INFERRED) | ? | ? | ? | 12785 | 40 | 146..8666 | open/none |

## Actor lifetimes

| identity | first f | last f | lifetime | auid (diagnostic) | miduid (diagnostic) |
|---|---|---|---|---|---|
| L1/auid44876 | 146 | 352 | complete | 44876 | 44880 |
| L1/auid44882 | 363 | 569 | complete | 44882 | 44886 |
| L1/auid44888 | 580 | 785 | complete | 44888 | 44892 |
| L1/auid44894 | 796 | 1002 | complete | 44894 | 44898 |
| L1/auid44900 | 977 | 1183 | complete | 44900 | 44904 |
| L1/auid44906 | 1193 | 1398 | complete | 44906 | 44910 |
| L1/auid44912 | 1409 | 1615 | complete | 44912 | 44916 |
| L1/auid44918 | 1625 | 1830 | complete | 44918 | 44922 |
| L1/auid44924 | 1841 | 2047 | complete | 44924 | 44928 |
| L1/auid44930 | 2057 | 2262 | complete | 44930 | 44934 |
| L1/auid44936 | 2273 | 2480 | complete | 44936 | 44940 |
| L1/auid44942 | 2490 | 2695 | complete | 44942 | 44946 |
| L1/auid44948 | 2706 | 2912 | complete | 44948 | 44952 |
| L1/auid44954 | 2922 | 3127 | complete | 44954 | 44958 |
| L1/auid44960 | 3138 | 3344 | complete | 44960 | 44964 |
| L1/auid44966 | 3354 | 3559 | complete | 44966 | 44970 |
| L1/auid44972 | 3570 | 3776 | complete | 44972 | 44976 |
| L1/auid44978 | 3786 | 3991 | complete | 44978 | 44982 |
| L1/auid44984 | 4002 | 4208 | complete | 44984 | 44988 |
| L1/auid44990 | 4218 | 4423 | complete | 44990 | 44994 |
| L1/auid44989 | 4434 | 4640 | complete | 44989 | 44985 |
| L1/auid44983 | 4650 | 4855 | complete | 44983 | 44979 |
| L1/auid44977 | 4866 | 5072 | complete | 44977 | 44973 |
| L1/auid44971 | 5082 | 5287 | complete | 44971 | 44967 |
| L1/auid44965 | 5298 | 5504 | complete | 44965 | 44961 |
| L1/auid44959 | 5514 | 5719 | complete | 44959 | 44955 |
| L1/auid44953 | 5730 | 5936 | complete | 44953 | 44949 |
| L1/auid44947 | 5946 | 6151 | complete | 44947 | 44943 |
| L1/auid44941 | 6162 | 6368 | complete | 44941 | 44937 |
| L1/auid44935 | 6378 | 6583 | complete | 44935 | 44931 |
| L1/auid44929 | 6594 | 6800 | complete | 44929 | 44925 |
| L1/auid44923 | 6810 | 7015 | complete | 44923 | 44919 |
| L1/auid44917 | 7026 | 7232 | complete | 44917 | 44913 |
| L1/auid44911 | 7242 | 7447 | complete | 44911 | 44907 |
| L1/auid44905 | 7458 | 7664 | complete | 44905 | 44901 |
| L1/auid44899 | 7674 | 7879 | complete | 44899 | 44895 |
| L1/auid44893 | 7890 | 8096 | complete | 44893 | 44889 |
| L1/auid44887 | 8106 | 8311 | complete | 44887 | 44883 |
| L1/auid44881 | 8322 | 8528 | complete | 44881 | 44877 |
| L1/auid44705 | 8538 | 8666 | complete | 44705 | 44701 |

## Findings

| Question | Finding | Evidence |
|---|---|---|
| MID sharing | none detected | every MID value belongs to exactly one lifetime |
| actor UID sharing | none detected | every actor UID value belongs to exactly one lifetime |
| Pooling | NO (evidence: no pool-return events) — LEGACY_LIMITATION | schema 1 has no stable identity; absence of pooling is asserted only from the absence of mode=pool events, never from UID uniqueness |
| Overlap | CONFIRMED — presentation-ownership defect (visiblepresentations>=2 while activetargets<=1) | max visiblepresentations=2, max activetargets=1, 26 frames >=2 in 1 run(s), longest=26 frames |
| Approach during exit | ACTOR TRANSFORM FROZEN | actor static in 40 exit(s), changed in 0; VisualMotion relative transform changed in 40 |
| Teardown separation | DESTROY_REQ-lastMAT_WRITE=[0]  ENDPLAY-DESTROY_REQ=[0] | Spec §11 check 3 requires >=1 full frame |
| Hidden phase | VIS_FALSE=0  HIDDEN_TRUE=0 | zero of both means no Hidden phase exists |
| Fade terminal value | final MAT_WRITE values=['0.000000'] | 0.000000 means the fade completes; >0 means truncation |

## Not determined by this tool

- Whether a terminal flash is visible. That is a rendered-viewport
  observation and no log field can establish it.
