### JSONL inspection — `malformed.jsonl`

- Structural result: **FAIL**
- Total records: 5
- Record types: `{'session_header': 1, 'trial': 2, 'block_summary': 1, 'session_footer': 1}`
- Trials: 2  |  unique: 2  |  duplicates: []  |  missing: [1]
- Stimulus counts: `{'GO': 1, 'NOGO': 1}`
- Outcome counts: `{'Hit': 1, 'Omission': 0, 'CorrectRejection': 1, 'Commission': 0, 'Void': 0}` (sum 2)
- Seeds: `[20260708]`  |  hashes: `['4922870221080512783']`
- Visual dur (min/max/mean s): `[0.401, 0.401, 0.401]`
- ITI (min/max/mean s): `[1.2, 1.2, 1.2]`
- responseAcceptedAfterVisualOffset=true: 0
- visualTerminatedByResponse=true: 0 (must be 0)
- Footer present: True  |  logger healthy: True  |  write failures: 0
- PII suspect hits: []

**Failures (structural):**
- Line 3: invalid JSON (Expecting property name enclosed in double quotes: line 1 column 3 (char 2))
- Missing trial indices in range: [1]
