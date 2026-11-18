#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Compile actual driver functions with deterministic fake registers/IRQs.

This is a single-CPU software regression, not an SD controller/card model.
The only source instrumentation is an injection hook immediately before the
PIO buffer store. Production function bodies otherwise remain unchanged.
"""
import argparse
import hashlib
import re
import subprocess
from pathlib import Path


def function(source, name):
    match = re.search(r"^static [^;{}]*\b" + name + r"\([^;{}]*\)\s*\{", source, re.M)
    if not match:
        raise ValueError("missing definition: " + name)
    depth = 1
    end = match.end()
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[match.start():end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--driver", type=Path)
    parser.add_argument("--baseline", action="store_true", help="require the original post-cancel write")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--emit-only", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    driver = args.driver or root / "arch/arm/src/stm32h7/stm32_sdmmc.c"
    raw = driver.read_bytes()
    source = raw.decode().replace("\r\n", "\n")
    names = ["stm32_recvfifo", "stm32_cancel", "stm32_endtransfer",
             "stm32_sdmmc_fifo_monitor", "stm32_recvsetup", "stm32_sendsetup",
             "stm32_sdmmc_interrupt"]
    bodies = [function(source, name) for name in names]
    store = "*priv->buffer++  = data.w;"
    assert bodies[0].count(store) == 1, "injection point changed; review the harness"
    bodies[0] = bodies[0].replace(store, "test_before_store(priv);\n          " + store)
    harness = Path(__file__).with_suffix(".c").read_text(encoding="utf-8")
    assert harness.count("/* DRIVER_FUNCTIONS */") == 1
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    generated = output / "sdmmc_pio.c"
    generated.write_text(harness.replace("/* DRIVER_FUNCTIONS */", "\n\n".join(bodies)), encoding="utf-8")
    print("driver_sha256=" + hashlib.sha256(raw).hexdigest(), flush=True)
    if args.emit_only:
        print("emitted actual function harness; NOT compiled or executed")
        return
    executable = output / "sdmmc_pio"
    command = [args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-Wno-pointer-to-int-cast", "-Wno-unused-parameter", "-Wno-unused-function",
               "-DBASELINE=" + str(int(args.baseline)), str(generated), "-o", str(executable)]
    if args.sanitize:
        command[1:1] = ["-g", "-fno-omit-frame-pointer", "-fsanitize=address,undefined"]
    subprocess.run(command, check=True)
    subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
