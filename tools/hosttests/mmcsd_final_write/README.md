# Final SDIO write and card-ready boundary, host characterization only

Related acceptance: [MCB #98](https://github.com/PRELUDE-Project/pp2-obc-mis/issues/98).
Kernel baseline: PRELUDE fork 7965c77ad63db5ed14506ca19f112ac6df04fa7e
(merged SD development candidate #1). This change adds no firmware path.
The separate early CMD24-poll harness still owns pre-setup error propagation.

Run from a checkout with Python 3 and a C compiler supporting UBSan:

    python3 -B tools/hosttests/mmcsd_final_write/run.py

The runner requires the exact recorded driver/header bytes, extracts the actual
mmcsd_writesingle, mmcsd_close, mmcsd_transferready and mmcsd_get_r1
definitions, and uses the actual R1 status constants. Transport, card and clock
are controlled doubles. The eight builds vary DMA capability, optional
write-completion notification and the polling-yield branch. No SD card,
board, file system, electrical timing or OS is exercised.

Observed at this baseline:

- DATAEND success returns one block and sets wrbusy, with no CMD13 seen.
  Closing the last reference succeeds without checking or clearing wrbusy.
- A later readiness check sends CMD13, can wait through PRG/RCV, clears wrbusy
  on accepted TRAN, and doesn't send another CMD24. In a deterministic tick
  model, persistent PRG/RCV hits the elapsed-time exit. A failing status poll or
  response, rejected R1, another card state, or absent media retains wrbusy
  when the earlier write had set it.
- In the optional completion-notification build, an injected notification
  timeout does not substitute for CMD13: the helper still checks card status.
  TRAN without READY_FOR_DATA is accepted by this baseline, not proposed as
  a stronger acceptance rule.
- An earlier DATAEND timeout returns the original error before setting wrbusy.
  Consequently the helper can later take its nonbusy fast path without any
  status query; that observation cannot certify a previous transfer stopped.
  A close lock error also preserves the reference count.

For an eventual reusable writer, keep card status under the same card/session
and owner, bound all notification and command waits under an explicit deadline,
and separately account for successful payload outcome, card readiness, IRQ/IDMA
retirement and source-buffer custody. A sector count, close or successful
CMD13 cannot prove all four; none alone should publish a clean record. The test
does not establish real timeout duration (individual calls are stubbed), safe
retry, actual IDMA drain, power-loss persistence, or why old MCB data differed.

## Proposed guard, exercised in a host fixture only

The same runner also tests a source-derived prototype in a temporary
translation unit. It changes no production driver, reserved storage, board
profile, FAT behavior or physical configuration. All baseline tests still run.

- An unsuccessful single-block data-event wait latches uncertainty in the
  fixture's session state, preserves the exact first error, and rejects later
  readiness or writes before any CMD13, partition switch or payload replay.
  Closing a reference does not clear the latch.
- A pending write only becomes card-ready once CMD13 reports both TRAN and
  READY_FOR_DATA. TRAN without READY_FOR_DATA uses the same bounded polling
  branch as PRG/RCV. The test models both later readiness and timeout.
- The latch only covers the illustrated data-event boundary. It is not a
  reliable complete owner/recovery state for the production driver, does not
  stop IDMA or prove physical drain or persistence, and intentionally has no
  clear or reset rule. Post-setup failures and multiple-block writes need a
  separate proven lifecycle, as does safe association with a fixed card/session
  and retained source buffer. Do not deploy this fixture or enable FAT from it.
