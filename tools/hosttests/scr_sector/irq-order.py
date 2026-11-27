#!/usr/bin/env python3
"""Generate two host fixtures from existing, pinned STM32H7 SDMMC sources."""
import hashlib
import sys
from pathlib import Path

PINS = (
    "345c943a1848cd2716a31b32ce6c3691caf169ce46643859630ee5be3bbf94b5",
    "f28baba2bcbed434535afe186a1605c2c501064af5a29ba4af69335e91a381fc",
)
MERGED_12P10_PIN = "ed660d4467dc4012fd648ac0f80200a0470ae78e3837b245d4c4d56d6b44d01f"


def extract(source):
    rx_start = source.index("static void stm32_recvdma(struct stm32_dev_s *priv)\n{")
    rx_end = source.index("\n}\n", rx_start) + 3
    irq_start = source.index("pending  = enabled & priv->xfrmask;")
    irq_end = source.index("/* Handle wait events", irq_start)
    return source[rx_start:rx_end], source[irq_start:irq_end].strip()


def main():
    args = sys.argv[1:]
    # 比較版を置き換える場合も、入力は統合コミットの実ファイルに限定する。
    merged = bool(args and args[0] == "--merged-12p10")
    if merged:
        args = args[1:]
    if len(args) != 3:
        raise SystemExit("usage: irq-order.py [--merged-12p10] BASELINE.c COMPARISON.c OUT_DIR")
    pins = (PINS[0], MERGED_12P10_PIN) if merged else PINS
    out = Path(args[2])
    out.mkdir(parents=True, exist_ok=True)
    template = Path(__file__).with_name("irq-order.c.in").read_text()
    for index, name in enumerate(("baseline", "error_first")):
        src = Path(args[index]).read_bytes()
        if hashlib.sha256(src).hexdigest() != pins[index]:
            raise SystemExit(f"unexpected source bytes for {name}")
        rx, irq = extract(src.decode("utf-8"))
        file = out / f"{name}.c"
        file.write_text(template.replace("@RX@", rx).replace("@IRQ@", irq)
                        .replace("@BASELINE@", str(1-index)))


if __name__ == "__main__":
    main()
