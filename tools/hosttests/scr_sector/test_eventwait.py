#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise the pinned STM32-to-MMC event handoff with RAM test doubles."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

SOURCES = (
    ("arch/arm/src/stm32h7/stm32_sdmmc.c",
     "affa479b1cfebd404dd880d7ffa19466a07457ba", ((813, 869), (3880, 3987))),
    ("drivers/mmcsd/mmcsd_sdio.c",
     "cc659e003afd1ec1fd4fee3d7b1a0b807bdc4270", ((1370, 1409),)),
)
FRAGMENT = "ec810a21abddb4ed0febe8601d9bca415519001b32bd8932c51b1865a754fdf7"
WRITE_READY = "-DCONFIG_MMCSD_SDIOWAIT_WRCOMPLETE"
MODES = (
    ("baseline", ()),
    ("write_ready", (WRITE_READY,)),
    ("review", (WRITE_READY, "-DCONFIG_SD_SECTOR_READ_REVIEW",
                "-DCONFIG_SD_RECEIVE_LEASE_ONLY_REVIEW")),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    args = parser.parse_args()
    parts = []
    for path, digest, ranges in SOURCES:
        raw = (args.repo / path).read_bytes()
        header = b"blob " + str(len(raw)).encode("ascii") + b"\0"
        if hashlib.sha1(header + raw).hexdigest() != digest:
            parser.error(f"Unexpected Git blob: {path}")
        lines = raw.splitlines(keepends=True)
        for first, last in ranges:
            parts.append(b"".join(lines[first - 1:last]))
    fragment = b"".join(parts)
    if hashlib.sha256(fragment).hexdigest() != FRAGMENT:
        parser.error("Unexpected extracted fragment")
    fixture = Path(__file__).with_name("eventwait_host.c")
    with tempfile.TemporaryDirectory(prefix="sd-eventwait-") as scratch:
        Path(scratch, "actual.inc").write_bytes(fragment)
        binary = Path(scratch, "eventwait")
        for opt in ("-O0", "-O2"):
            for mode, switches in MODES:
                subprocess.run(
                    ["cc", "-std=c11", opt, "-Wall", "-Wextra", "-Werror",
                     "-fsanitize=undefined", "-fno-sanitize-recover=all",
                     *switches, "-I", scratch, str(fixture), "-o", str(binary)],
                    check=True,
                )
                print(f"{mode} {opt}", flush=True)
                subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
