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
test='\nint main(void){\n void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);\n struct stm32_dev_s*p=m;uint8_t out[512],*arena;struct scr_lease_key key;\n unsigned faults[]={0,STM32_SDMMC_STA_DATAEND,STM32_SDMMC_STA_DPSMACT,STM32_SDMMC_STA_CPSMACT,STM32_SDMMC_STA_IDMATE,STM32_SDMMC_STA_DABORT,STM32_SDMMC_STA_DTIMEOUT,STM32_SDMMC_STA_DCRCFAIL};unsigned count=0;\n for(unsigned sector=0;sector<2;sector++)for(unsigned f=0;f<8;f++){\n inject_icr=0;fresh(p);\n if(sector){key=begin(p);setup(p);command(p);terminal(p,0);}\n else{assert(stm32_scr_claim(&p->dev,&arena,&key)==0);p->blocksize=8;assert(stm32_dmarecvsetup(&p->dev,arena,8)==0);uint32_t r;assert(stm32_sendcmd(&p->dev,SD_CMD55,0x10000)==0);regs[9]=55;regs[10]=0;regs[0]=0;assert(stm32_recvshortcrc(&p->dev,SD_CMD55,&r)==0);assert(stm32_sendcmd(&p->dev,SD_ACMD51,0)==0);regs[9]=51;assert(stm32_recvshortcrc(&p->dev,SD_ACMD51,&r)==0);p->waitevents=SDIOWAIT_TRANSFERDONE;regs[0]=STM32_SDMMC_STA_DATAEND;regs[6]=0;regs[7]=8;regs[4]=STM32_SDMMC_DCTRL_DTDIR|STM32_SDMMC_DCTRL_8BYTES;assert(stm32_sdmmc_interrupt(0,NULL,p)==0);}\n memset(p->scrlease.arena,0x5a,512);memset(out,0xa5,512);inject_icr=faults[f];\n int r=sector?stm32_sector_finish(&p->dev,key,0,out):stm32_scr_finish(&p->dev,key,0,(uint32_t*)out);\n if(f==0){assert(r==0);for(unsigned j=0;j<(sector?512u:8u);j++)assert(out[j]==0x5a);for(unsigned j=sector?512u:8u;j<512;j++)assert(out[j]==0xa5);assert(p->scrlease.state==(sector?SCR_LEASE_RETIRED:SCR_LEASE_HANDOFF));}\n else{assert(r==-EIO&&p->scrlease.state==SCR_LEASE_QUARANTINED);for(unsigned j=0;j<512;j++)assert(out[j]==0xa5);assert(regs[5]==0);inject_icr=0;regs[0]=0;r=sector?stm32_sector_finish(&p->dev,key,0,out):stm32_scr_finish(&p->dev,key,0,(uint32_t*)out);assert(r==-EIO);for(unsigned j=0;j<512;j++)assert(out[j]==0xa5);}\n count++;\n }\n printf("PASS %u final ICR readback cases: success copies exact length; stuck DATAEND/new active/error bits quarantine without output and remain closed after clearing input. Host only.\\n",count);munmap(m,4096);return 0;\n}\n'
driver=(fixture/'driver.c').read_text()
old='if(off==2)regs[0]&=~v;'
new='if(off==2){regs[0]&=~v;if(v==STM32_SDMMC_STA_DATAEND)regs[0]|=inject_icr;}'
assert driver.count(old)==1
driver=driver.replace(old,new)
(b/'driver-injected.c').write_text('static unsigned inject_icr;\n'+driver)
c='#define main original_fixture_main\n#include "'+str(b/'driver-injected.c')+'"\n#undef main\n'+body+'\n'+test
(b/'test.c').write_text(c)
cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast','-Wno-return-type','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(fixture),'-I'+str(fixture/'stubs'),'-I'+str(root/'include/nuttx'),str(b/'test.c'),'-o',str(b/'test')]
subprocess.run(cmd,check=True)
r=subprocess.run([str(b/'test')],check=True,capture_output=True,text=True)
(b/'result.log').write_text(r.stdout);print(r.stdout)
