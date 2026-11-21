#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run the pinned SDMMC wait-setup checks on a host, with no board access."""

import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

DRIVER = "arch/arm/src/stm32h7/stm32_sdmmc.c"
SOURCE_BLOB = "affa479b1cfebd404dd880d7ffa19466a07457ba"
BODY_SHA256 = "9ee353fb4bb849ca9d0917866d8b5f418044f2ad18ee77fc58a89036074ff1fa"
START = 3740
END = 3857


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True, type=Path)
    options = parser.parse_args()

    source = (options.repo / DRIVER).read_bytes()
    header = b"blob " + str(len(source)).encode("ascii") + b"\0"
    if hashlib.sha1(header + source).hexdigest() != SOURCE_BLOB:
        parser.error("driver source does not match the fixed NuttX blob")
    body = b"".join(source.splitlines(keepends=True)[START - 1 : END])
    if hashlib.sha256(body).hexdigest() != BODY_SHA256:
        parser.error("extracted function does not match the fixed source")

    fixture = Path(__file__).resolve().with_name("waitenable_host.c")
    with tempfile.TemporaryDirectory(prefix="sd-waitenable-") as temp:
        scratch = Path(temp)
        (scratch / "waitenable.inc").write_bytes(body)
        for opt in ("-O0", "-O2"):
            binary = scratch / ("waitenable" + opt)
            subprocess.run(
                [
                    "cc", "-std=c11", opt, "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=undefined", "-fno-sanitize-recover=all",
                    "-I", str(scratch), str(fixture), "-o", str(binary),
                ],
                check=True,
            )
            print(opt, flush=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
