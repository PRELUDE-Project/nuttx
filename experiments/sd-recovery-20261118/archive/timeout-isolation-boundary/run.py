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
 struct stm32_dev_s*p=m;uint8_t out[512],*arena;struct scr_lease_key key,other;
 unsigned patterns[]={STM32_SDMMC_STA_DATAEND,STM32_SDMMC_STA_DPSMACT,STM32_SDMMC_STA_CPSMACT,STM32_SDMMC_STA_IDMATE,STM32_SDMMC_STA_DTIMEOUT,STM32_SDMMC_STA_DCRCFAIL,0xffffffffu};
 for(unsigned iteration=0;iteration<100;iteration++) for(unsigned sector=0;sector<2;sector++) for(unsigned e=0;e<7;e++){
  fresh(p);
  if(sector){key=begin(p);setup(p);}
  else {assert(stm32_scr_claim(&p->dev,&arena,&key)==0);p->blocksize=8;assert(stm32_dmarecvsetup(&p->dev,arena,8)==0);}
  p->waitevents=SDIOWAIT_TIMEOUT|SDIOWAIT_TRANSFERDONE|SDIOWAIT_ERROR;
  p->waitwdog=true;p->waitmask=SDIOWAIT_TIMEOUT;regs[0]=STM32_SDMMC_STA_DPSMACT;
  unsigned before=notifications;
  stm32_eventtimeout((wdparm_t)p);
  assert(p->scrlease.state==SCR_LEASE_QUARANTINED && p->scrlease.result==-ETIMEDOUT);
  assert(!p->waitwdog && !p->buffer && !p->remaining && !p->receivecnt && !p->unaligned_rx);
  assert(p->xfrmask==0 && p->waitmask==0 && regs[1]==0 && notifications==before+1);
  assert(regs[5]==STM32_SDMMC_IDMACTRLR_IDMAEN); /* Isolation is not a fabricated stop. */
  regs[0]=patterns[e];uintptr_t snapshot[16];memcpy(snapshot,regs,sizeof regs);
  struct stm32_dev_s saved;memcpy(&saved,p,sizeof saved);
  stm32_eventtimeout((wdparm_t)p);assert(stm32_sdmmc_interrupt(0,NULL,p)==0);
  assert(memcmp(snapshot,regs,sizeof regs)==0 && memcmp(&saved,p,sizeof saved)==0 && notifications==before+1);
  memset(p->scrlease.arena,0x77,sizeof p->scrlease.arena); /* Model possible residual DMA, not CPU output. */
  memset(out,0xa5,sizeof out);
  int ret=sector?stm32_sector_finish(&p->dev,key,-EBUSY,out):stm32_scr_finish(&p->dev,key,-EBUSY,(uint32_t*)out);
  assert(ret==-ETIMEDOUT);
  for(unsigned i=0;i<sizeof out;i++)assert(out[i]==0xa5);
  assert(stm32_scr_claim(&p->dev,&arena,&other)==-EBUSY);
  assert(stm32_sector_claim(&p->dev,1,4096,true,&arena,&other)==-EBUSY);
 }
 munmap(m,4096);puts("PASS 1400 actual watchdog->endwait->seal cases: late IRQ/timer inert, IDMA left enabled, output preserved, same-boot claims rejected; MMIO/OS mocked");
 return 0;
}
'''
c='#define main original_fixture_main\n#include "'+str(fixture/'driver.c')+'"\n#undef main\n'+body+'\n'+test
(b/'test.c').write_text(c)
cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast','-Wno-return-type','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(fixture/'stubs'),'-I'+str(root/'include/nuttx'),str(b/'test.c'),'-o',str(b/'test')]
subprocess.run(cmd,check=True)
r=subprocess.run([str(b/'test')],check=True,capture_output=True,text=True)
(b/'result.log').write_text(r.stdout);print(r.stdout)
