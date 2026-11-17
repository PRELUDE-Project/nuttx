#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import hashlib,subprocess,tempfile,itertools,re
p=Path(__file__).resolve().parent;root=p.parents[2]
if not __debug__: raise SystemExit('Optimized Python is not supported')
assert hashlib.sha256((root/'drivers/mmcsd/mmcsd_sdio.c').read_bytes()).hexdigest()==(p/'source.sha256').read_text().strip(), 'Stale extracted harness'

# Compare the reviewed production definitions, not just a whole-file hash.
# Historical baseline_writesingle and transport doubles intentionally remain
# frozen models, and must not be replaced by this comparison.
def definition(text, name):
 matches = list(re.finditer(r'\bstatic\s+(?:int|bool|ssize_t)\s+'+re.escape(name)+r'\([^;{}]*?\)\s*\{',text))
 if len(matches)!=1: raise ValueError('Expected one definition '+name)
 match=matches[0]
 depth=1
 for token in re.finditer(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]',text[match.end():]):
  if token.group()=='{': depth+=1
  elif token.group()=='}': depth-=1
  if depth==0: return text[match.start():match.end()+token.end()]
 raise ValueError('Unclosed definition '+name)
source=(root/'drivers/mmcsd/mmcsd_sdio.c').read_text()
for filename,names in {
 'test.c':['mmcsd_writesingle'],
 'caller-test.c':['mmcsd_writesingle','mmcsd_write','mmcsd_wrprotected'],
 'poll-test.c':['mmcsd_writesingle','mmcsd_write','mmcsd_wrprotected','mmcsd_sendcmdpoll'],
}.items():
 fixture=(p/filename).read_text()
 for name in names:
  if definition(source,name)!=definition(fixture,name):
   raise SystemExit('Stale function '+name+' in '+filename+'; regenerate and review')

with tempfile.TemporaryDirectory(prefix='write-earlypoll-') as tmp:
 def run(src,flags):
  exe=str(Path(tmp)/'test')
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-fsanitize=undefined','-fno-sanitize-recover=all',*flags,str(p/src),'-o',exe],check=True)
  subprocess.run([exe],check=True)
 for cfg in itertools.product((0,1),repeat=3):
  run('test.c',['-D'+n for on,n in zip(cfg,['CONFIG_SDIO_DMA','CONFIG_ARCH_HAVE_SDIO_PREFLIGHT','CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE']) if on])
 for ro in (0,1):
  run('caller-test.c',['-DCONFIG_SDIO_DMA','-DCONFIG_ARCH_HAVE_SDIO_PREFLIGHT']+(['-DCONFIG_MMCSD_READONLY'] if ro else []))
 for dma in (0,1):
  run('poll-test.c',['-DCONFIG_SDIO_DMA'] if dma else [])
print('PASS host-only early CMD24 error propagation; no hardware or card operations.')
