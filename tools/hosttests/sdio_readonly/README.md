# SDIO read-only regression snapshot

This extracted-source host test checks mmcsd_wrprotected, single/multiple/general write entry points and geometry. With CONFIG_MMCSD_READONLY, writable media must still return EPERM before any lower transfer operation and report write-disabled geometry. Without it, existing card-lock/software/mechanical protection behavior is retained. Lower operations and OS are mocked; the test is not a physical card test. Re-extract source when the implementation changes.

```
cc -std=c11 tools/hosttests/sdio_readonly/snapshot.c -o /tmp/sdio-rw-test
/tmp/sdio-rw-test
cc -std=c11 -DCONFIG_MMCSD_READONLY tools/hosttests/sdio_readonly/snapshot.c -o /tmp/sdio-ro-test
/tmp/sdio-ro-test
```

For an initial non-writing diagnostic image also disable CONFIG_MMCSD_IOCSUPPORT and write-capable test applications. The raw ioctl CMD6 switch path is not covered by the block write-protection predicate. Read-only block access is not a guarantee of no volatile protocol-state changes during enumeration or a substitute for wiring/power checks, card data protection, hardware testing or flashing approval.
