#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Read-only positive/negative checks for the fixture correspondence guard."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

base = Path(__file__).resolve().parent
root = base.parents[2]
spec = importlib.util.spec_from_file_location('regenerate', base/'regenerate.py')
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)
text = 'static int demo(void) { /* } */ const char *x="}"; if (x) { return 0; } return 1; }'
a, b = gen.span(text, 'demo')
assert text[a:b] == text
with tempfile.TemporaryDirectory(prefix='scr-extract-check-') as directory:
    sandbox = Path(directory)
    target = sandbox/'tools/hosttests/scr_sector'
    shutil.copytree(base, target, ignore=shutil.ignore_patterns('__pycache__'))
    for source in json.loads((base/'sources.json').read_text()):
        dest = sandbox/source
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(root/source, dest)
    command = [sys.executable, str(target/'regenerate.py')]
    assert subprocess.run(command, capture_output=True).returncode == 0
    fixture = target/'driver.c'
    original = fixture.read_text()
    fixture.write_text(original+'\n/* injected stale fixture */\n')
    assert subprocess.run(command, capture_output=True).returncode != 0
    fixture.write_text(original)
    source = sandbox/'arch/arm/src/stm32h7/stm32_sdmmc.c'
    original_source = source.read_text()
    a, b = gen.span(original_source, 'scr_closed')
    changed = original_source[:a] + original_source[a:b].replace('{', '{ /* changed source */', 1) + original_source[b:]
    source.write_text(changed)
    manifest = target/'sources.json'
    hashes = json.loads(manifest.read_text())
    hashes['arch/arm/src/stm32h7/stm32_sdmmc.c'] = hashlib.sha256(source.read_bytes()).hexdigest()
    manifest.write_text(json.dumps(hashes, indent=2)+'\n')
    assert subprocess.run(command, capture_output=True).returncode != 0, 'Hash-only update must not pass'
print('PASS extraction parser, clean correspondence, stale fixture and hash-only source update rejection')
