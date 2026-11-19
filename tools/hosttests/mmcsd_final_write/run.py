#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compile actual upper MMC/SD write and completion functions against doubles."""
import hashlib
import itertools
import os
import re
import subprocess
import tempfile
from pathlib import Path

from prototype import connected

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def extract(text, name):
    pattern = rf"(?m)^static (?:int|ssize_t) {name}\([^;{{}}]*?\)\s*\{{"
    matches = list(re.finditer(pattern, text))
    if len(matches) != 1:
        raise ValueError(f"Expected exactly one definition of {name}")
    start = matches[0]
    depth = 1
    tokens = r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]'
    for token in re.finditer(tokens, text[start.end():]):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if not depth:
                return text[start.start():start.end() + token.end()]
    raise ValueError(f"Unclosed definition of {name}")


def main():
    if not __debug__:
        raise SystemExit("Optimized Python is not supported")
    for row in (HERE / "source.sha256").read_text().splitlines():
        expected, filename = row.split()
        path = (HERE / filename).resolve(strict=True)
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise SystemExit(f"Source changed: review {path.relative_to(ROOT)}")
    c_source = (ROOT / "drivers/mmcsd/mmcsd_sdio.c").read_text()
    h_source = (ROOT / "drivers/mmcsd/mmcsd_sdio.h").read_text()
    names = ("STATE_MASK", "STATE_SHIFT", "STATE_STBY", "STATE_TRAN",
             "STATE_RCV", "STATE_PRG", "READYFORDATA", "CARDISLOCKED", "WPVIOLATION", "ERRORMASK")
    defines = []
    for name in names:
        line = re.findall(rf"(?m)^#\s*define\s+MMCSD_R1_{name}\s+(.*)$", h_source)
        if len(line) != 1:
            raise SystemExit(f"Unexpected cardinality of MMCSD_R1_{name}")
        defines.append(f"#define MMCSD_R1_{name} {line[0]}")
    extracts = [extract(c_source, name) for name in
                ("mmcsd_get_r1", "mmcsd_transferready", "mmcsd_writesingle", "mmcsd_close")]
    with tempfile.TemporaryDirectory(prefix="mmcsd-final-write-") as tmp:
        work = Path(tmp)
        (work / "status-bits.h").write_text("\n".join(defines) + "\n")
        mods = ("CONFIG_SDIO_DMA", "CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE",
                "CONFIG_MMCSD_CHECK_READY_STATUS_WITHOUT_SLEEP")
        for proposed in (False, True):
            source = connected(extracts) if proposed else extracts
            (work / "actual.c.inc").write_text("\n\n".join(source) + "\n")
            for cfg in itertools.product((False, True), repeat=len(mods)):
                flags = [f"-D{name}" for name, value in zip(mods, cfg) if value]
                if proposed:
                    flags.append("-DCANDIDATE")
                exe = work / "case"
                subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-g", "-Wall",
                                "-Wextra", "-Werror", "-Wno-unused-function",
                                "-Wno-unused-parameter", "-fsanitize=undefined",
                                "-fno-sanitize-recover=all", *flags, "-I", tmp,
                                str(HERE / "test.c"), "-o", str(exe)], check=True)
                subprocess.run([str(exe)], check=True)
            name = "host prototype" if proposed else "unmodified baseline"
            print(f"PASS 8 configurations: {name}; no hardware")


if __name__ == "__main__":
    main()
