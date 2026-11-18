from pathlib import Path
import subprocess
b=Path(__file__).resolve().parent
r=b.parent/'scr-claim-null-guard/product'
f=r/'tools/hosttests/scr_sector'
s=(f/'upper.c').read_text()
s=s.replace('#include <stdint.h>','#include <stdint.h>\n#include <setjmp.h>')
pos=s.index('static int stm32_sector_claim')
s=s[:pos]+'int mmcsd_sector_readonce(struct sdio_dev_s*,uint64_t,uint8_t*,size_t);\nstatic jmp_buf abandoned; static int hook_at, abandon_now, hooks;\nstatic void hook(int n){if(n!=hook_at)return;uint8_t out[512];memset(out,0xc3,512);int before[6];memcpy(before,calls,sizeof calls);assert(g_sector_attempted&&depth==0);assert(mmcsd_sector_readonce(&gdev,0,out,512)==-EBUSY);assert(memcmp(before,calls,sizeof calls)==0);for(unsigned i=0;i<512;i++)assert(out[i]==0xc3);hooks++;if(abandon_now)longjmp(abandoned,1);}\n'+s[pos:]
for i in range(6):s=s.replace(f'calls[{i}]++;',f'calls[{i}]++;hook({i});')
s=s[:s.index('int main(void)')]+r"""
int main(void){unsigned count=0;static uint8_t out[512];
for(hook_at=0;hook_at<6;hook_at++)for(abandon_now=0;abandon_now<2;abandon_now++){
reset();stage=0;hooks=0;memset(out,0xa5,512);memset(arena,0x5a,512);
if(setjmp(abandoned)==0){int ret=mmcsd_sector_readonce(&gdev,0,out,512);assert(!abandon_now&&ret==0);}
assert(hooks==1&&depth==0&&g_sector_attempted);
int before[6];memcpy(before,calls,sizeof calls);
assert(mmcsd_sector_readonce(&gdev,0,out,512)==-EBUSY);
assert(memcmp(before,calls,sizeof calls)==0);
for(unsigned i=0;i<512;i++)assert(out[i]==(abandon_now?0xa5:0x5a));count++;
}printf("PASS %u actual readonce reentry/abandon models: admission stays consumed, nested and later calls cannot reach lower hooks. No OS task deletion or DMA proof.\n",count);return 0;}
"""
(b/'test.c').write_text(s)
subprocess.run(['cc','-std=c11','-fsanitize=undefined','-fno-sanitize-recover=all',str(b/'test.c'),'-o',str(b/'test')],check=True)
p=subprocess.run([str(b/'test')],capture_output=True,text=True);(b/'result.log').write_text(p.stdout+p.stderr);print(p.stdout+p.stderr);assert p.returncode==0
