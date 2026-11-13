#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Pinned extracted-function regressions; never connect to a device."""
from pathlib import Path
import hashlib, json, subprocess, tempfile
p=Path(__file__).resolve().parent
root=p.parents[2]
for name, expected in json.loads((p/'sources.json').read_text()).items():
    assert hashlib.sha256((root/name).read_bytes()).hexdigest()==expected, 'Stale extracted harness: '+name
with tempfile.TemporaryDirectory(prefix='scr-sector-host-') as tmp:
    common=['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(p/'stubs'),'-I'+str(root/'include/nuttx')]
    for name in ['driver','integration','upper']:
        exe=str(Path(tmp)/name)
        subprocess.run(common+['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast',str(p/(name+'.c')),'-o',exe],check=True)
        subprocess.run([exe],check=True)
    for enabled in [False,True]:
        exe=str(Path(tmp)/('cli-on' if enabled else 'cli-off'))
        subprocess.run(common+(['-DCONFIG_SD_SECTOR_READ_REVIEW=1'] if enabled else [])+[str(p/'cli.c'),'-o',exe],check=True)
        for mode in (range(5) if enabled else [0]):
            subprocess.run([exe,str(mode)],check=True)
print('PASS pinned host tests. MMIO, timers and OS/device APIs are mocked; no physical-stop proof.')
