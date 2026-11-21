#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check pinned SDMMC wait setup and terminal timeout paths on a host."""

import argparse
import hashlib
from pathlib import Path
import subprocess
import tempfile

DRIVER = "arch/arm/src/stm32h7/stm32_sdmmc.c"
SOURCE_BLOB = "affa479b1cfebd404dd880d7ffa19466a07457ba"
CASES = (
    (
        "waitenable_host.c",
        "waitenable.inc",
        ((3740, 3857),),
        "9ee353fb4bb849ca9d0917866d8b5f418044f2ad18ee77fc58a89036074ff1fa",
    ),
    (
        "watchdog_terminal_host.c",
        "functions.inc",
        ((912, 920), (1947, 1982), (2002, 2026)),
        "b5d91fd80b045900fa11ac15510699388c10de9de997cf08fb0b04261b07967f",
    ),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True, type=Path)
    options = parser.parse_args()

    source = (options.repo / DRIVER).read_bytes()
    header = b"blob " + str(len(source)).encode("ascii") + b"\0"
    if hashlib.sha1(header + source).hexdigest() != SOURCE_BLOB:
        parser.error("driver source does not match the fixed NuttX blob")
    source_lines = source.splitlines(keepends=True)
    bodies = {}
    for fixture_name, include, ranges, digest in CASES:
        body = b"".join(
            b"".join(source_lines[start - 1 : end]) for start, end in ranges
        )
        if hashlib.sha256(body).hexdigest() != digest:
            parser.error(f"{fixture_name}: extracted functions do not match the fixed source")
        bodies[include] = body

    fixtures = Path(__file__).resolve().parent
    with tempfile.TemporaryDirectory(prefix="sd-waitenable-") as temp:
        scratch = Path(temp)
        for fixture_name, include, _, _ in CASES:
            (scratch / include).write_bytes(bodies[include])
            fixture = fixtures / fixture_name
            for opt in ("-O0", "-O2"):
                binary = scratch / (fixture.stem + opt)
                subprocess.run(
                    [
                        "cc", "-std=c11", opt, "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=undefined", "-fno-sanitize-recover=all",
                        "-I", str(scratch), str(fixture), "-o", str(binary),
                    ],
                    check=True,
                )
                print(f"{fixture_name} {opt}", flush=True)
                subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
