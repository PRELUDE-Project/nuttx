# SCR-to-sector ownership transition

Current PR #1 HEAD 431fd0ea0f2a4f9a178fa46d9a53c46235a2433f was reread unchanged and draft. Uses the verified product copy under scr-claim-null-guard/product, not the old 627d0ec root.

Static review found that HANDOFF deliberately allows stm32_sector_claim to obtain a new lease; it does not enforce the SCR owner's task identity. The upper contract says trusted task-only one-shot API, not same-task continuation, and stores successful identification persistently. Therefore absence of a same-task check is not by itself a demonstrated bug. Adding such a restriction would change the interface and could incorrectly forbid a legitimate separate diagnostic call. No speculative restriction was added.

New regression uses actual successful SCR claim, DMA setup, CMD55/ACMD51 response, IRQ and finish to reach HANDOFF (not a seeded state), then sector claims by task IDs 1, 2 and 3. The new key increments generation and records the new task. An old SCR key cannot finish the sector even when the task ID matches; the former task cannot set up or finish another task's sector. New owner timeout returns ETIMEDOUT, preserves all 512 caller-output bytes, quarantines and forbids another claim. Three paths passed UBSan. IRQ/MMIO/critical/task identity are host doubles; real concurrent worker dispatch and physical stop are not proven.

Initial test accidentally claimed LBA 0 while existing command helper sends LBA 1, triggering the expected command assertion. Corrected test to claim LBA 1; no product change for this fixture error. Result.log is final successful run.

Remaining boundary: task IDs may be reused after task exit, and an earlier numeric task identity is not an OS lifetime token. This test does not solve external deletion/PID reuse, join semantics or hardware DMA cessation. Persistent ownership redesign must address those separately; forced unquarantine is not justified. Formal H753 config, target build and power HOLD remain unresolved. No installation, remote write or hardware operation.
