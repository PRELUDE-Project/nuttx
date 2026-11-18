#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Regenerate explicit production extracts; preserve named host doubles.
Default checks only. --write requires subsequent diff review and full run.py.
"""
from pathlib import Path
import argparse, hashlib, json, re

def span(text, name):
    pattern = r"\b(?:static\s+)?(?:inline\s+)?(?:int|bool|void|size_t|uint32_t|sdio_eventset_t)\s+" + re.escape(name) + r"\([^;{}]*?\)\s*\{"
    matches = list(re.finditer(pattern, text))
    if len(matches) != 1:
        raise ValueError(f"Expected one definition for {name}, found {len(matches)}")
    m = matches[0]
    depth = 1
    for tok in re.finditer(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[m.end():]):
        if tok.group() == '{': depth += 1
        elif tok.group() == '}': depth -= 1
        if depth == 0:
            return m.start(), m.end() + tok.end()
    raise ValueError('Unclosed function ' + name)

def render(base):
    root = base.parents[2]
    mapping = json.loads((base/'extracts.json').read_text())
    result = {}
    for fixture, functions in mapping.items():
        text = (base/'templates'/(fixture+'.c.in')).read_text()
        for name, source in functions.items():
            token = '@EXTRACT:' + name + '@'
            if text.count(token) != 1:
                raise ValueError('Missing or duplicate template token '+token)
            src = (root/source).read_text()
            a,b = span(src,name)
            text = text.replace(token, src[a:b])
        if '@EXTRACT:' in text:
            raise ValueError('Unresolved token')
        result[fixture+'.c'] = text
    sources = sorted({s for v in mapping.values() for s in v.values()} | {'include/nuttx/scr_lease_review.h', 'include/nuttx/sdio.h', 'drivers/mmcsd/mmcsd_sdio.h', 'arch/arm/src/stm32h7/hardware/stm32h7x3xx_sdmmc.h'})
    result['sources.json'] = json.dumps({s:hashlib.sha256((root/s).read_bytes()).hexdigest() for s in sources},indent=2)+'\n'
    return result

if __name__ == '__main__':
    args=argparse.ArgumentParser()
    args.add_argument('--write', action='store_true')
    options=args.parse_args()
    base=Path(__file__).resolve().parent
    for name,text in render(base).items():
        path=base/name
        if options.write: path.write_text(text)
        elif path.read_text()!=text: raise SystemExit('Stale generated fixture: '+name+'; regenerate and review')
    print('Regenerated; review diff and run all tests.' if options.write else 'PASS source/template/extract correspondence')
