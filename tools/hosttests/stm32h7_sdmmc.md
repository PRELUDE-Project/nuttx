# STM32H7 PIO receive cancellation regression

Candidate based on NuttX 12.10.0 commit
`6fc5b3e9dc51dcda46d99519f72b7f2ec6db2e64`.

The FIFO monitor previously ran in HPWORK without excluding cancellation or
the SDMMC interrupt. Disabling the device interrupt did not stop an executing
worker. A worker could store to the caller's buffer after cancellation had
returned, or resume after the next transfer had installed another buffer.

The candidate uses a separate timer watchdog for the residual receive FIFO.
Ordinary IRQ critical sections serialize monitor, PIO word updates, final
copy, setup and cancellation. Monitor service has fewer than 32 bytes to
consume, at most eight FIFO reads. The regular IRQ drain retains its original
FIFO-empty termination, with a critical section for each complete word update.
An existing monitor deadline is preserved by subsequent FIFO interrupts.
Pending monitor timers are revoked on cancellation, completion, reset and the
next setup. Small-transfer internal IDMA setup/copy remains in place.

This reasoning is for one CPU and ordinary interrupts that honor NuttX's
critical section. It is not a synchronization design for SMP, another core
sharing the peripheral, or a zero-latency interrupt accessing driver state.
`wd_cancel()` alone does not join an executing callback. The timer ISR and
the shared critical sections are essential to the lifetime argument.

## Host regression

```sh
python3 tools/hosttests/stm32h7_sdmmc.py --output /path/to/new/host-output
python3 tools/hosttests/stm32h7_sdmmc.py --sanitize --output /path/to/new/sanitized-output
```

The script extracts seven actual C function bodies from the selected driver,
injects one deterministic cancellation hook immediately before a PIO store,
and compiles them with fake registers, IRQ exclusion, work/timer and event
notifications. It prints the exact input source SHA-256. To reproduce the
original race, use `--baseline --driver <original stm32_sdmmc.c>`.

Checks cover the store/cancel interleaving, pending timer revocation,
receive/send transition, completion, fractional FIFO tails, an unchanged
already-armed deadline, RXFIFOHF with DATAEND, the actual DATAEND copy from the
internal IDMA buffer, and buffer canaries. The fake scheduler is deterministic;
it is not a complete model of the NuttX scheduler, interrupt controller or SD
peripheral. Reset is statically reviewed but not executed by this harness.

Native execution, full driver/target builds, timer/SDMMC priorities and worst
case execution time must be recorded separately. These checks do not establish
IDMA/cache quiescence, card-side STOP completion, write cancellation safety,
media preservation, or permission to deploy this candidate. No card, COM port,
target flash, or physical board is accessed by the host regression.
