# Early CMD24 poll error regression

Run `python3 tools/hosttests/mmcsd_write_earlypoll/run.py` with a C11 host compiler. Tests use extracted actual upper functions and transport/device doubles; source hash prevents stale extracts running unnoticed. Regenerate and review extracts after relevant source changes, before updating the hash.

Only the first CMD24 poll (before this transfer's buffer/wait setup) now preserves a negative return. No cancel/reset or retry is added. CMD24 may already have reached the card; an error does not prove rollback, idle, safe retry or unchanged media. Post-setup CMD24 and PIO setup ignored-return defects remain because their caller-buffer lifetime cannot be fixed by an unproven cancel.

Tests compare baseline and candidate ordering/results across DMA/preflight/WRCOMPLETE configurations and capabilities; actual upper write stops after first negative result and unlocks. Actual sendcmdpoll tests distinguish SENDCMD from WAITRESPONSE failure. Readonly rejects before command/setup. All device activity is simulated. No write diagnostic is enabled by this change.
