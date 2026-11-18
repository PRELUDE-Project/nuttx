from pathlib import Path
import subprocess
b=Path(__file__).resolve().parent
root=b.parent/'nuttx';f=root/'tools/hosttests/scr_sector'
prefix=(b.parent/'timeout-isolation-boundary/test.c').read_text().split('\nint main(void){')[0]
test=r'''
int main(void){
 void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);
 struct stm32_dev_s*p=m;uint8_t*arena;struct scr_lease_key key;
 unsigned attempts=0;
 for(unsigned sector=0;sector<2;sector++){
 fresh(p);if(sector){key=begin(p);setup(p);}else{assert(stm32_scr_claim(&p->dev,&arena,&key)==0);p->blocksize=8;assert(stm32_dmarecvsetup(&p->dev,arena,8)==0);}
 p->waitevents=SDIOWAIT_TIMEOUT;stm32_eventtimeout((wdparm_t)p);
 struct stm32_dev_s saved;memcpy(&saved,p,sizeof saved);uintptr_t snapshot[16];memcpy(snapshot,regs,sizeof regs);
 for(unsigned owner=1;owner<3;owner++){
 current_owner=owner;
 for(unsigned cmd=0;cmd<65536;cmd++){assert(stm32_sendcmd(&p->dev,cmd,0xffffffffu)==-EBUSY);attempts++;}
 uint8_t *buffers[]={NULL,p->scrlease.arena,g_sdmmcdev1.scrlease.arena,g_sdmmcdev2.scrlease.arena,p->rxfifo,p->sdmmc_rxbuffer};
 size_t sizes[]={0,1,8,32,512,513};
 for(unsigned x=0;x<6;x++)for(unsigned y=0;y<6;y++){assert(stm32_dmarecvsetup(&p->dev,buffers[x],sizes[y])<0);attempts++;}
 uint32_t response=0xabcdef01;assert(stm32_recvshortcrc(&p->dev,MMCSD_CMD17,&response)==-EBUSY && response==0xabcdef01);attempts++;
 assert(memcmp(&saved,p,sizeof saved)==0 && memcmp(snapshot,regs,sizeof regs)==0);
 }
 }
 munmap(m,4096);printf("PASS %u closed-lease admission attempts preserve device/arena/MMIO; owners 1/2, SCR/sector, aliases/sizes/all 16-bit commands\n",attempts);return 0;
}
'''
(b/'test.c').write_text(prefix+'\n'+test)
cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast','-Wno-return-type','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(f/'stubs'),'-I'+str(root/'include/nuttx'),str(b/'test.c'),'-o',str(b/'test')]
subprocess.run(cmd,check=True)
r=subprocess.run([str(b/'test')],capture_output=True,text=True,check=True);(b/'result.log').write_text(r.stdout);print(r.stdout)
