#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Pinned extracted-function regressions; never connect to a device."""
from pathlib import Path
import hashlib, json, subprocess, tempfile, sys
p=Path(__file__).resolve().parent
if not __debug__: raise SystemExit("Optimized Python is not supported")
subprocess.run([sys.executable, str(p/"regenerate.py")], check=True)
subprocess.run([sys.executable, str(p/"test_regenerate.py")], check=True)
root=p.parents[2]
for name, expected in json.loads((p/'sources.json').read_text()).items():
    assert hashlib.sha256((root/name).read_bytes()).hexdigest()==expected, 'Stale extracted harness: '+name
with tempfile.TemporaryDirectory(prefix='scr-sector-host-') as tmp:
    common=['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(p/'stubs'),'-I'+str(root/'include/nuttx')]
    for name in ['driver','integration','upper']:
        exe=str(Path(tmp)/name)
        subprocess.run(common+['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast',str(p/(name+'.c')),'-o',exe],check=True)
        subprocess.run([exe],check=True)
    for enabled in [False,True]:
        exe=str(Path(tmp)/('cli-on' if enabled else 'cli-off'))
        subprocess.run(common+(['-DCONFIG_SD_SECTOR_READ_REVIEW=1'] if enabled else [])+[str(p/'cli.c'),'-o',exe],check=True)
        for mode in (range(5) if enabled else [0]):
            result=subprocess.run([exe,str(mode)],check=True,capture_output=True,text=True)
            summary="Read summary:" in result.stdout
            assert summary == (enabled and mode==0), "CRC summary must appear only after successful sector read"
            print(result.stdout, end="")
print('PASS pinned host tests. MMIO, timers and OS/device APIs are mocked; no physical-stop proof.')
