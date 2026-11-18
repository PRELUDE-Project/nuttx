from pathlib import Path
import subprocess,sys,json
b=Path(__file__).resolve().parent
root=b.parent/'nuttx'
fixture=root/'tools/hosttests/scr_sector'
sys.path.insert(0,str(fixture))
from regenerate import span
subprocess.run([sys.executable,str(fixture/'regenerate.py')],check=True)
src=(root/'arch/arm/src/stm32h7/stm32_sdmmc.c').read_text()
a,z=span(src,'stm32_eventtimeout')
body=src[a:z]
test=r'''
int main(void){
 void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);
 struct stm32_dev_s*p=m;uint8_t out[512],*arena;struct scr_lease_key key,bad;
 unsigned count=0;
 for(unsigned sector=0;sector<2;sector++)for(unsigned timeout=0;timeout<2;timeout++)for(unsigned invalid=0;invalid<3;invalid++)for(unsigned err=0;err<3;err++)for(unsigned dst=0;dst<3;dst++){
  fresh(p);if(sector){key=begin(p);setup(p);}else{assert(stm32_scr_claim(&p->dev,&arena,&key)==0);p->blocksize=8;assert(stm32_dmarecvsetup(&p->dev,arena,8)==0);}
  if(timeout){p->waitevents=SDIOWAIT_TIMEOUT;p->waitwdog=true;regs[0]=STM32_SDMMC_STA_DPSMACT;stm32_eventtimeout((wdparm_t)p);}
  bad=key;if(invalid==0)bad.generation++;if(invalid==1)bad.owner++;if(invalid==2)current_owner=2;
  struct stm32_dev_s saved;memcpy(&saved,p,sizeof saved);uintptr_t mmio[16];memcpy(mmio,regs,sizeof regs);unsigned n=notifications,bar=barriers;memset(out,0xa5,sizeof out);
  void*output=dst==0?out:dst==1?NULL:p->scrlease.arena;int input=err==0?0:err==1?-EBUSY:-EIO;
  int r=sector?stm32_sector_finish(&p->dev,bad,input,output):stm32_scr_finish(&p->dev,bad,input,output);
  assert(r==-ESTALE);assert(!memcmp(&saved,p,sizeof saved));assert(!memcmp(mmio,regs,sizeof regs));assert(notifications==n&&barriers==bar);
  for(unsigned i=0;i<512;i++)assert(out[i]==0xa5);
  current_owner=1;r=sector?stm32_sector_finish(&p->dev,key,-EBUSY,out):stm32_scr_finish(&p->dev,key,-EBUSY,(uint32_t*)out);
  assert(r==(timeout?-ETIMEDOUT:-EBUSY));assert(p->scrlease.state==SCR_LEASE_QUARANTINED);count++;
 }
 printf("PASS %u finish authority cases: stale generation/key owner/current task cannot copy, seal, change MMIO, wake or barrier; legitimate error preserved. Host doubles only.\n",count);munmap(m,4096);return 0;
}
'''
c='#define main original_fixture_main\n#include "'+str(fixture/'driver.c')+'"\n#undef main\n'+body+'\n'+test
(b/'test.c').write_text(c)
cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast','-Wno-return-type','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(fixture/'stubs'),'-I'+str(root/'include/nuttx'),str(b/'test.c'),'-o',str(b/'test')]
subprocess.run(cmd,check=True)
r=subprocess.run([str(b/'test')],check=True,capture_output=True,text=True)
(b/'result.log').write_text(r.stdout);print(r.stdout)
