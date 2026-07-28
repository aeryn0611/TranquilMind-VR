# Stage 1 trace report (v2) — session `L1`

Source log: `Documentation/Stage1/evidence/pie_authoritative_capture.log`
Schema: **1**  — identity is **LEGACY_INFERRED**
Sessions in file: 1 (this report covers **one**)
Events in session: 9140   Actor identities: 39

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
| L1 | schema 1 (LEGACY_INFERRED) | ? | ? | ? | 9140 | 39 | 288964..295283 | open/none |

## Actor lifetimes

| identity | first f | last f | lifetime | auid (diagnostic) | miduid (diagnostic) |
|---|---|---|---|---|---|
| L1/auid52110 | 288964 | 289199 | complete | 52110 | 52106 |
| L1/auid52104 | 289212 | 289447 | complete | 52104 | 52100 |
| L1/auid52098 | 289460 | 289698 | complete | 52098 | 52094 |
| L1/auid52092 | 289711 | 289951 | complete | 52092 | 52088 |
| L1/auid52086 | 289922 | 290161 | complete | 52086 | 52082 |
| L1/auid52080 | 290172 | 290255 | complete | 52080 | 52076 |
| L1/auid52074 | 290256 | 290266 | complete | 52074 | 52070 |
| L1/auid52068 | 290265 | 290275 | complete | 52068 | 52064 |
| L1/auid52062 | 290274 | 290284 | complete | 52062 | 52058 |
| L1/auid52056 | 290283 | 290293 | complete | 52056 | 52052 |
| L1/auid52050 | 290292 | 290302 | complete | 52050 | 52045 |
| L1/auid52043 | 290301 | 290311 | complete | 52043 | 52038 |
| L1/auid52036 | 290310 | 290320 | complete | 52036 | 52032 |
| L1/auid52030 | 290319 | 290329 | complete | 52030 | 52025 |
| L1/auid52022 | 290328 | 290338 | complete | 52022 | 52017 |
| L1/auid52015 | 290337 | 290347 | complete | 52015 | 52010 |
| L1/auid51884 | 290346 | 290356 | complete | 51884 | 51880 |
| L1/auid51878 | 290355 | 290365 | complete | 51878 | 51874 |
| L1/auid51872 | 290364 | 290374 | complete | 51872 | 52112 |
| L1/auid52109 | 295171 | 295283 | complete | 52109 | 52105 |
| L1/auid52103 | 290382 | 290726 | complete | 52103 | 52099 |
| L1/auid52097 | 290738 | 290973 | complete | 52097 | 52093 |
| L1/auid52091 | 290986 | 291227 | complete | 52091 | 52087 |
| L1/auid52085 | 291238 | 291475 | complete | 52085 | 52081 |
| L1/auid52079 | 291488 | 291726 | complete | 52079 | 52075 |
| L1/auid52073 | 291737 | 291972 | complete | 52073 | 52069 |
| L1/auid52067 | 291986 | 292219 | complete | 52067 | 52063 |
| L1/auid52061 | 292231 | 292462 | complete | 52061 | 52057 |
| L1/auid52055 | 292474 | 292705 | complete | 52055 | 52051 |
| L1/auid52049 | 292716 | 292947 | complete | 52049 | 52044 |
| L1/auid52041 | 292959 | 293194 | complete | 52041 | 52037 |
| L1/auid52035 | 293205 | 293440 | complete | 52035 | 52031 |
| L1/auid52029 | 293452 | 293679 | complete | 52029 | 52023 |
| L1/auid52021 | 293691 | 293927 | complete | 52021 | 52016 |
| L1/auid52014 | 293940 | 294177 | complete | 52014 | 52009 |
| L1/auid51883 | 294190 | 294427 | complete | 51883 | 51879 |
| L1/auid47528 | 294440 | 294678 | complete | 47528 | 47353 |
| L1/auid47345 | 294690 | 294917 | complete | 47345 | 47323 |
| L1/auid47112 | 294930 | 295159 | complete | 47112 | 52112 |

## Findings

| Question | Finding | Evidence |
|---|---|---|
| MID sharing | UE_ID_RECYCLED | 1 MID value(s) reused across NON-overlapping lifetimes — index recycling after GC, not shared state: 52112: auid51872 [290364..290374], auid47112 [294930..295159] |
| actor UID sharing | none detected | every actor UID value belongs to exactly one lifetime |
| Pooling | NO (evidence: no pool-return events) — LEGACY_LIMITATION | schema 1 has no stable identity; absence of pooling is asserted only from the absence of mode=pool events, never from UID uniqueness |
| Overlap | CONFIRMED — presentation-ownership defect (visiblepresentations>=2 while activetargets<=1) | max visiblepresentations=2, max activetargets=1, 58 frames >=2 in 15 run(s), longest=30 frames |
| Approach during exit | ACTOR TRANSFORM FROZEN | actor static in 37 exit(s), changed in 0; VisualMotion relative transform changed in 37; 1 conflated key(s) excluded (LEGACY_LIMITATION) |
| Teardown separation | DESTROY_REQ-lastMAT_WRITE=[0]  ENDPLAY-DESTROY_REQ=[0] | Spec §11 check 3 requires >=1 full frame |
| Hidden phase | VIS_FALSE=0  HIDDEN_TRUE=0 | zero of both means no Hidden phase exists |
| Fade terminal value | final MAT_WRITE values=['0.000000'] | 0.000000 means the fade completes; >0 means truncation |
| Excluded from exit metrics | LEGACY_LIMITATION — 1 conflated identity key(s) | schema-1 auid recycling merged two presentations under one key; those keys are excluded rather than producing a meaningless cross-actor frame difference: auid52109 [295171..295283] |

## Not determined by this tool

- Whether a terminal flash is visible. That is a rendered-viewport
  observation and no log field can establish it.
