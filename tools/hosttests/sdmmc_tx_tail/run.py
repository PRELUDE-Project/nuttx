#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Extract and regression-test the pinned repository TX FIFO function offline."""
from pathlib import Path
import argparse
import hashlib
import os
import re
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo', type=Path, default=HERE.parents[2],
                    help='NuttX repository root (default: relative to this runner)')
args = parser.parse_args()
source = args.repo / 'arch/arm/src/stm32h7/stm32_sdmmc.c'
raw = source.read_bytes()
expected = (HERE / 'source.sha256').read_text().split()[0]
actual = hashlib.sha256(raw).hexdigest()
if actual != expected:
    raise SystemExit(f'driver hash mismatch: expected {expected}, observed {actual}; review source changes before repinning')
text = raw.decode('utf-8')
match = re.search(r'^static void stm32_sendfifo\(struct stm32_dev_s \*priv\)\s*\{', text, re.M)
if not match:
    raise SystemExit('stm32_sendfifo definition not found')
end, depth = match.end(), 1
while depth:
    depth += (text[end] == '{') - (text[end] == '}')
    end += 1
function = text[match.start():end]
with tempfile.TemporaryDirectory(prefix='sdmmc-tx-tail-') as directory:
    work = Path(directory)
    (work / 'sendfifo.inc').write_text(function + '\n')
    compiler = os.environ.get('CC', 'cc')
    subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-g', '-O1', '-fno-omit-frame-pointer',
                    '-fsanitize=address,undefined', '-I', str(work),
                    str(HERE / 'test.c'), '-o', str(work / 'test')],
                   cwd=work, check=True)
    env = os.environ.copy()
    # LeakSanitizer is unsupported in some traced CI environments. The test
    # frees each allocation; ASan bounds checks and UBSan remain enabled.
    env['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1'
    env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
    subprocess.run([str(work / 'test')], cwd=work, env=env, check=True)
print(f'Pinned source SHA256 verified: {actual}')
