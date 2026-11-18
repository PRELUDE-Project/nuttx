from pathlib import Path
import sys,subprocess
b=Path(__file__).resolve().parent;r=b.parent/'nuttx';f=r/'tools/hosttests/scr_sector';sys.path.insert(0,str(f));from regenerate import span
subprocess.run([sys.executable,str(f/'regenerate.py')],check=True)
src=(r/'arch/arm/src/stm32h7/stm32_sdmmc.c').read_text();a,z=span(src,'stm32_attach');body=src[a:z]
d=(f/'driver.c').read_text();needle='struct stm32_dev_s{struct sdio_dev_s dev;';assert needle in d;d=d.replace(needle,needle+'int nirq;');(b/'driver.c').write_text(d)
head='''
#define CONFIG_SD_SCR_CONTAINMENT_REVIEW 1
#define STM32_SDMMC_MASK_RESET 0
#define STM32_SDMMC_ICR_ALLFLAGS 0x1fe00fff
static int attach_result,attach_calls,enable_calls;
static int irq_attach(int n, int(*fn)(int,void*,void*),void*arg){assert(depth>0);assert(n==65&&fn==stm32_sdmmc_interrupt&&arg==observed);attach_calls++;return attach_result;}
static void up_enable_irq(int n){assert(depth>0&&n==65&&attach_calls==1);assert(regs[1]==0&&(regs[0]&STM32_SDMMC_ICR_ALLFLAGS)==0);enable_calls++;}
'''
test='''
int main(void){
 void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);struct stm32_dev_s*p=m;unsigned n=0;
 int states[]={SCR_LEASE_IDLE,SCR_LEASE_ACTIVE,SCR_LEASE_HANDOFF,SCR_LEASE_QUARANTINED,SCR_LEASE_RETIRED};int errors[]={0,-EIO,-ENOMEM};
 for(unsigned s=0;s<5;s++)for(unsigned init=0;init<2;init++)for(unsigned err=0;err<3;err++){
 fresh(p);p->nirq=65;p->scrlease.state=states[s];p->scrlease.owner=1;p->scrinitializing=init;attach_result=errors[err];attach_calls=enable_calls=0;regs[0]=0xffffffff;regs[1]=0x1234;uintptr_t saved[16];memcpy(saved,regs,sizeof regs);struct stm32_dev_s prev;memcpy(&prev,p,sizeof prev);
 int ret=stm32_attach(&p->dev);assert(depth==0);
 if(s||init){assert(ret==-EBUSY&&attach_calls==0&&enable_calls==0&&!memcmp(saved,regs,sizeof regs));}
 else if(err){assert(ret==errors[err]&&attach_calls==1&&enable_calls==0&&!memcmp(saved,regs,sizeof regs));}
 else{assert(ret==0&&attach_calls==1&&enable_calls==1&&regs[1]==0);}
 assert(!memcmp(&prev,p,sizeof prev));n++;
 }
 printf("PASS %u attach admission/failure cases: exact errno, no enable/clear on rejection, controller clear before NVIC enable on success. OS/MMIO doubles only.\\n",n);munmap(m,4096);return 0;
}
'''
(b/'test.c').write_text('#define main fixture_main\n#include "driver.c"\n#undef main\n'+head+body+test)
cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast','-Wno-return-type','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(f),'-I'+str(f/'stubs'),'-I'+str(r/'include/nuttx'),str(b/'test.c'),'-o',str(b/'test')];subprocess.run(cmd,check=True);x=subprocess.run([str(b/'test')],check=True,capture_output=True,text=True);(b/'result.log').write_text(x.stdout);print(x.stdout)
