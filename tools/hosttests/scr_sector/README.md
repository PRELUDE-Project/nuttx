# Single-sector review host regressions

Run `python3 tools/hosttests/scr_sector/run.py` from a writable test workspace with a C11 host compiler. No target or hardware tools are used. Driver tests require Linux mmap of a free region at 0x24000000; failure to map is a test-environment failure, not permission to replace an existing mapping.

These are pinned extracted-function harnesses, not a full NuttX simulator. `sources.json` refuses to run stale extracts after the corresponding production sources change. Regenerate and review extracts before updating those hashes. Driver/upper integration models MMIO, timers, OS dispatch and parts of block/wait setup. CLI device APIs are entirely stubbed. No result proves real IRQ drain, IDMA physical stop, electrical safety or hardware compatibility.

Cases include normal copy, command/R1/data/IDMA errors, delayed completion, owner/generation and reentry refusal, strict media notifications, CSD admission, decimal LBA parsing and feature-off CLI rejection. The standalone upper harness uses stubbed lower calls; integration.c couples extracted upper and lower paths. No sector content is sent externally.

## Regenerating and checking extracts

`extracts.json` lists every copied production function and its source. The
`templates` directory contains only the test scaffolding and explicit extraction
markers. Host doubles remain deliberately visible there; they are not presented
as production implementations. Run:

```
python3 tools/hosttests/scr_sector/regenerate.py --write
git diff -- tools/hosttests/scr_sector
python3 tools/hosttests/scr_sector/run.py
```

Review changed definitions, signatures, dependent helpers and all host doubles
before accepting regeneration. The generator does not prove that a changed
production dependency is correctly modeled. Without `--write`, it checks both
template/source correspondence and source hashes without modifying files. The manifest also pins the three
command/register-constant source headers; review constant models after changes. The
runner always performs that check before compiling, and rejects optimized Python.
Do not update `sources.json` alone to make a stale fixture run. Negative tests
exercise stale-fixture and hash-only-update rejection.

The refreshed CLI fixture includes the current CRC32 summary. It tests the
standard `123456789` CRC vector and empty input, and the runner checks that a read
summary appears only for successful sector reads, not SCR-only/failed/feature-off
paths. A CRC summary is not a comparison against expected card data.

These are the full regressions in this fixture directory, not a target build or
a complete OS/cancellation simulation. The templates retain their documented
MMIO, timer, scheduler, block/wait, and card-protocol doubles.
