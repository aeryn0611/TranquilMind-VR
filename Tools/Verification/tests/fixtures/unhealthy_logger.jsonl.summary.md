### JSONL inspection — `unhealthy_logger.jsonl`

- Structural result: **FAIL**
- Total records: 6
- Record types: `{'session_header': 1, 'trial': 3, 'block_summary': 1, 'session_footer': 1}`
- Trials: 3  |  unique: 3  |  duplicates: []  |  missing: []
- Stimulus counts: `{'GO': 2, 'NOGO': 1}`
- Outcome counts: `{'Hit': 1, 'Omission': 1, 'CorrectRejection': 1, 'Commission': 0, 'Void': 0}` (sum 3)
- Seeds: `[20260708]`  |  hashes: `['4922870221080512783']`
- Visual dur (min/max/mean s): `[0.401, 0.401, 0.401]`
- ITI (min/max/mean s): `[1.0, 1.2, 1.1]`
- responseAcceptedAfterVisualOffset=true: 0
- visualTerminatedByResponse=true: 0 (must be 0)
- Footer present: True  |  logger healthy: False  |  write failures: 2
- PII suspect hits: []

**Failures (structural):**
- Logger footer healthy != true (healthy=False).
- Logger reported writeFailures=2 (expected 0).
