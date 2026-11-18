# Experimental H753 SCR containment

`CONFIG_SD_SCR_CONTAINMENT_REVIEW` is an explicit, default-off review option. It is not hardware-qualified. Existing configurations keep the previous driver and upper SCR path when it is disabled.

The option is restricted to H753ZI, uniprocessor, no high-priority interrupts, and configured D-cache off. Admission also checks runtime cache state, known silicon revisions and the full aligned persistent arena in AXI SRAM. Y/W and unknown revisions are rejected; the Y/W AXI SRAM erratum workaround is not implemented.

For SCR, a per-controller persistent arena replaces the caller's temporary buffer. Normal completion requires owned CMD55/ACMD51 response collection, clean single-buffer DATAEND, inactive CPSM/DPSM, zero DCOUNT, expected length/base/mode and closed software producers. Only the qualified task completion copies eight bytes and permits a new generation. On errors the arena remains quarantined and further controller operations are rejected. There is no abort recovery or automatic reset.

The normal-completion basis is RM0433 Rev9 §55.5.6, Single buffered channel: DATAEND follows IDMA transfer and DPSM completion. Errata screening uses ES0392 Rev16. Register observations and host models do not establish physical abort completion, old IRQ drain or board safety.

## Validation and limitations

Independent host tests connected the actual SCR upper functions, receive setup, command/poll/R1 functions, ISR and task finish. PIO and IDMA builds passed IRQ-first/R1-first, two normal generations, R1/CRC failure isolation, arena boundary/DTCM rejection and idle-path setup/cancel checks. The peripheral, card, DMA and OS behavior are mocked. The committed host test is an extracted source snapshot, not a replacement for rerunning extraction after driver changes.

Compile and run the snapshot on Linux (requires an unused mapping at 0x24000000):

```
cc -std=c11 -Iinclude/nuttx -Itools/hosttests/scr_containment tools/hosttests/scr_containment/snapshot.c -o /tmp/scr-pio-test
/tmp/scr-pio-test
cc -std=c11 -DCONFIG_STM32H7_SDMMC_IDMA -Iinclude/nuttx -Itools/hosttests/scr_containment tools/hosttests/scr_containment/snapshot.c -o /tmp/scr-idma-test
/tmp/scr-idma-test
```

H753 diagnostic ARM cross-build passed. Enabled/disabled PIO/IDMA preprocessing and compilation were compared independently against the tested candidate and previous product source. The diagnostic configuration is not asserted to be the user's adopted physical configuration. No board was flashed and no SD card was accessed. Wiring, power, silicon revision, target configuration, card protection and any required flashing approval remain prerequisites for real testing.
