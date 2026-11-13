# Single-sector review host regressions

Run `python3 tools/hosttests/scr_sector/run.py` from a writable test workspace with a C11 host compiler. No target or hardware tools are used. Driver tests require Linux mmap of a free region at 0x24000000; failure to map is a test-environment failure, not permission to replace an existing mapping.

These are pinned extracted-function harnesses, not a full NuttX simulator. `sources.json` refuses to run stale extracts after the corresponding production sources change. Regenerate and review extracts before updating those hashes. Driver/upper integration models MMIO, timers, OS dispatch and parts of block/wait setup. CLI device APIs are entirely stubbed. No result proves real IRQ drain, IDMA physical stop, electrical safety or hardware compatibility.

Cases include normal copy, command/R1/data/IDMA errors, delayed completion, owner/generation and reentry refusal, strict media notifications, CSD admission, decimal LBA parsing and feature-off CLI rejection. The standalone upper harness uses stubbed lower calls; integration.c couples extracted upper and lower paths. No sector content is sent externally.
