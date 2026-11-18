# SCR observation regression

Extracted-source host snapshots, not hardware tests. Compile each C file with `cc -std=c11 -Iinclude -fsanitize=undefined file.c -o test` and run. Re-extract after source changes. MMIO, task context and OS are mocked; upper-snapshot also mocks transfer/finish.

Experimental CONFIG_SD_SCR_CONTAINMENT_REVIEW plus CONFIG_DEBUG_FS_ERROR exposes a trusted-kernel, read-only pre-finish observation through existing ferr/SYSLOG. DEBUG_FEATURES, DEBUG_ERROR, DEBUG_FS and output routing must also be configured. Without DEBUG_FS_ERROR, upper SCR adds no observation reads. Logs identify version and generation, never authorize stopping or reusing memory. Reading six registers and logging changes timing and stack use.

RM0433 Rev9 DCNTR is limited to completed/held transfers, so this diagnostic omits DCOUNT, FIFO and ICR. STA samples bracketing readable configuration are not an atomic peripheral snapshot; matching values do not prove quiescence. API output must be aligned writable normal RAM, never MMIO, and not concurrently accessed. It is not an untrusted ioctl API. No card writes, reset, acknowledgement, cancellation or initialization is performed by the snapshot function. Real-board operation remains unverified.
