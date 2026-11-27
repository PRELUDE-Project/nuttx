#!/usr/bin/env python3
"""Generate two host fixtures from existing, pinned STM32H7 SDMMC sources."""
import hashlib
import sys
from pathlib import Path

PINS = (
    "345c943a1848cd2716a31b32ce6c3691caf169ce46643859630ee5be3bbf94b5",
    "f28baba2bcbed434535afe186a1605c2c501064af5a29ba4af69335e91a381fc",
)


def extract(source):
    rx_start = source.index("static void stm32_recvdma(struct stm32_dev_s *priv)\n{")
    rx_end = source.index("\n}\n", rx_start) + 3
    irq_start = source.index("pending  = enabled & priv->xfrmask;")
    irq_end = source.index("/* Handle wait events", irq_start)
    return source[rx_start:rx_end], source[irq_start:irq_end].strip()


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: irq-order.py BASELINE.c ERROR_FIRST.c OUT_DIR")
    out = Path(sys.argv[3])
    out.mkdir(parents=True, exist_ok=True)
    template = Path(__file__).with_name("irq-order.c.in").read_text()
    for index, name in enumerate(("baseline", "error_first")):
        src = Path(sys.argv[index + 1]).read_bytes()
        if hashlib.sha256(src).hexdigest() != PINS[index]:
            raise SystemExit(f"unexpected source bytes for {name}")
        rx, irq = extract(src.decode("utf-8"))
        file = out / f"{name}.c"
        file.write_text(template.replace("@RX@", rx).replace("@IRQ@", irq)
                        .replace("@BASELINE@", str(1-index)))


if __name__ == "__main__":
    main()
