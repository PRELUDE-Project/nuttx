# IDMA disable transition boundary

Baseline: PRELUDE-Project/nuttx PR #1, 627d0ec82c4d33fad252547bc65462e4d2d3effd; current metadata re-read: Draft/open. Driver SHA256: 77b99406c6cbce2575e920f973a0a2d0ab0c5660ce2cd528505234c12b327bff.

Actual SCR/sector finish functions are extracted by the existing source-correspondence generator. New MMIO double injection occurs immediately after the IDMACTRLR=0 write, not at final ICR clearing. 16 UBSan cases cover SCR and 512-byte sector success, DPSMACT or CPSMACT becoming set, DATAEND disappearing, DCOUNT becoming nonzero, IDMATE appearing, and IDMACTRLR reading back IDMAEN or IDMABMODE. All pass. Success copies only 8 or 512 bytes. Each fault returns EIO, preserves the caller output and quarantines the lease. Clearing the injection does not reopen the lease.

This verifies software readback rejection, not physical DMA cessation, peripheral register timing or a valid abort protocol. The injection is a model, not evidence that these transitions occur on silicon. It does not justify changing IDMAEN while DPSMACT is active: the first observed status was inactive, and this test only exercises a later inconsistent observation. Timeout/cancel isolation remains unchanged. No runtime product defect requiring a new patch was exposed; no speculative abort or recovery was added.

No official adopted H753 configuration, ARM object, ELF or hardware result is produced by this test. kconfig-frontends authorization and physical power HOLD remain separate. The test is saved locally only; no remote write or installation occurred.

Reproduce: python finish-idma-transition/run.py from this recovery directory with existing host cc/Python. run.py regenerates the approved fixture from the fixed local source and compiles the isolated injection with UBSan.
