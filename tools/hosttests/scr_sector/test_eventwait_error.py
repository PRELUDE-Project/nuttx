#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Actual eventwait error return must seal before caller finish. Host only."""
from pathlib import Path
import subprocess, tempfile
from regenerate import span
p=Path(__file__).resolve().parent
root=p.parents[2]
source=(root/'arch/arm/src/stm32h7/stm32_sdmmc.c').read_text()
a,z=span(source,'stm32_eventwait')
fixture=(p/'driver.c').read_text()
fixture=fixture[:fixture.index('int main(void)')]
old='static int nxsem_wait_uninterruptible(int*s){assert(*s>0);--*s;return 0;}'
assert fixture.count(old)==1, 'Wait double changed; review instrumentation'
fixture=fixture.replace(old,'static int injected_wait; static int nxsem_wait_uninterruptible(int*s){(void)s;return injected_wait;}')
# Negative returns are injected, not real OS cancellation. EINTR is defensive:
# the real uninterruptible helper normally consumes it rather than returning it.
main='\nint main(void){void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);struct stm32_dev_s*p=m;unsigned count=0;int errors[]={-ECANCELED,-EINTR,-EIO};\nfor(unsigned i=0;i<3;i++)for(unsigned timed=0;timed<2;timed++){\nfresh(p);struct scr_lease_key key=begin(p);setup(p);command(p);p->waitevents=SDIOWAIT_TRANSFERDONE|SDIOWAIT_TIMEOUT|SDIOWAIT_ERROR;p->waitwdog=true;injected_wait=errors[i];\nif(timed)stm32_endwait(p,SDIOWAIT_TIMEOUT);\nassert(stm32_eventwait(&p->dev)==SDIOWAIT_ERROR);assert(depth==0);\n#if CANDIDATE\nassert(p->scrlease.state==SCR_LEASE_QUARANTINED);assert(!p->scrlease.cpu_destination&&!p->buffer&&!p->xfrmask&&!p->waitmask&&!p->waitwdog);assert(p->scrlease.result==(timed?-ETIMEDOUT:-EIO));\n#else\nassert(p->scrlease.state==(timed?SCR_LEASE_QUARANTINED:SCR_LEASE_ACTIVE));\n#endif\nuint8_t out[512];memset(out,0xa5,512);terminal(p,0);\n#if CANDIDATE\nassert(stm32_sector_finish(&p->dev,key,0,out)==(timed?-ETIMEDOUT:-EIO));for(unsigned j=0;j<512;j++)assert(out[j]==0xa5);\n#endif\ncount++;}\nprintf("PASS candidate=%d %u negative wait models; no task deletion or physical stop proof.\\n",CANDIDATE,count);}\n'
text=fixture+'\n#define stm32_dumpsamples(p) ((void)0)\n'+source[a:z]+main
with tempfile.TemporaryDirectory(prefix='scr-eventwait-error-') as tmp:
    src=Path(tmp)/'test.c';exe=Path(tmp)/'test'
    src.write_text(text)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast','-fsanitize=undefined','-fno-sanitize-recover=all','-DCANDIDATE=1','-I'+str(p),'-I'+str(p/'stubs'),'-I'+str(root/'include/nuttx'),str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
