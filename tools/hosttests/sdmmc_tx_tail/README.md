# STM32H7 PIO TX FIFO tail regression

Run from a NuttX checkout with Python 3 and an installed C11 compiler supporting AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
python3 tools/hosttests/sdmmc_tx_tail/run.py
```

The runner locates the repository relative to its own path, verifies the complete `arch/arm/src/stm32h7/stm32_sdmmc.c` SHA-256 in `source.sha256`, extracts the actual `stm32_sendfifo` definition, and compiles/runs the host harness in a temporary directory. `--repo PATH` selects another checkout; `CC` selects a compiler executable. No network, repository writes, installation or hardware operations occur. Source hash mismatch stops before compilation. After any driver change, independently review applicability before updating the pin; do not silently skip the check.

The 269 cases cover lengths 1, 2, 3, 4, 5, 511 and 512, pausing at every possible FIFO word boundary (including initially full and fully drained). They verify remaining counts, repeated full-FIFO calls, subsequent resumption, exact FIFO bytes, zero tail padding, unchanged source bytes and no duplicate word after completion. Each source allocation is exactly its requested length, so ASan detects out-of-bounds full-word tail reads. UBSan and ASan run with halt-on-error. Leak detection is disabled for compatibility with traced CI environments; no leak-check claim is made. All allocations are explicitly freed. A little-endian host is required and checked.

The register operations are FIFO-capacity stubs. This verifies the extracted function's software copying and bounds behavior only. It does not exercise a real IRQ, scheduler, DMA, card, bus timing, physical cancellation, cache coherence or write completion. It does not enable a write diagnostic or establish permission to write media. ARM compilation and hardware validation are separate evidence.

The full-source pin deliberately requires explicit review after unrelated edits too. The extraction assumes the pinned function's present brace/comment structure; it is not a general C parser.
