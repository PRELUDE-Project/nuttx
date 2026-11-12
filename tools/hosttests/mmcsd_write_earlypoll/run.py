#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import hashlib,subprocess,tempfile,itertools
p=Path(__file__).resolve().parent;root=p.parents[2]
assert hashlib.sha256((root/'drivers/mmcsd/mmcsd_sdio.c').read_bytes()).hexdigest()==(p/'source.sha256').read_text().strip(), 'Stale extracted harness'
with tempfile.TemporaryDirectory(prefix='write-earlypoll-') as tmp:
 def run(src,flags):
  exe=str(Path(tmp)/'test')
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-fsanitize=undefined',*flags,str(p/src),'-o',exe],check=True)
  subprocess.run([exe],check=True)
 for cfg in itertools.product((0,1),repeat=3):
  run('test.c',['-D'+n for on,n in zip(cfg,['CONFIG_SDIO_DMA','CONFIG_ARCH_HAVE_SDIO_PREFLIGHT','CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE']) if on])
 for ro in (0,1):
  run('caller-test.c',['-DCONFIG_SDIO_DMA','-DCONFIG_ARCH_HAVE_SDIO_PREFLIGHT']+(['-DCONFIG_MMCSD_READONLY'] if ro else []))
 for dma in (0,1):
  run('poll-test.c',['-DCONFIG_SDIO_DMA'] if dma else [])
print('PASS host-only early CMD24 error propagation; no hardware or card operations.')
