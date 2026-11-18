# Early CMD24 poll error regression

Run `python3 tools/hosttests/mmcsd_write_earlypoll/run.py` with a C11 host compiler. Tests use extracted actual upper functions and transport/device doubles; source hash prevents stale extracts running unnoticed. Regenerate and review extracts after relevant source changes, before updating the hash.

Only the first CMD24 poll (before this transfer's buffer/wait setup) now preserves a negative return. No cancel/reset or retry is added. CMD24 may already have reached the card; an error does not prove rollback, idle, safe retry or unchanged media. Post-setup CMD24 and PIO setup ignored-return defects remain because their caller-buffer lifetime cannot be fixed by an unproven cancel.

Tests compare baseline and candidate ordering/results across DMA/preflight/WRCOMPLETE configurations and capabilities; actual upper write stops after first negative result and unlocks. Actual sendcmdpoll tests distinguish SENDCMD from WAITRESPONSE failure. Readonly rejects before command/setup. All device activity is simulated. No write diagnostic is enabled by this change.

## Reviewed source-pin refresh

The current production source at `1ecf2a5d949a5c52b15d5cdf34ab86272d7131d4`
was compared with the original pin's commit before refreshing the whole-file
hash. The exercised function definitions are byte-identical; intervening changes
were outside those definitions. This is an explicit applicability review, not
permission to ignore future source mismatches. No runtime write capability,
recovery operation or hardware test was added.

The original pin was from `9eb58e2ba9ecef070a1146f83bf74055fdf2225a`.
All current `mmcsd_writesingle`, `mmcsd_write`, `mmcsd_wrprotected` and
`mmcsd_sendcmdpoll` extracts were checked against actual source. The runner now
also compares those full definitions before compiling; changing only the pin
cannot conceal a stale copied function. Historical `baseline_writesingle` and
transport doubles remain intentionally fixed. If the production functions
change, review and replace the corresponding definitions in each fixture,
preserve the baseline/doubles, review dependencies, then update the source pin
and rerun every configuration. Optimized Python is rejected.
