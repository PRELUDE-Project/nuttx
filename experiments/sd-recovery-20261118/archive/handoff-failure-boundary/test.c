
#define main original_fixture_main
#include "${RECOVERY_ROOT}/nuttx/tools/hosttests/scr_sector/driver.c"
#undef main
int main(void){
 void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);
 struct stm32_dev_s*p=m;
 for(unsigned fault=0;fault<12;fault++){
 fresh(p);uint8_t*arena;struct scr_lease_key key,next;uint32_t response,scr[2];
 assert(stm32_scr_claim(&p->dev,&arena,&key)==0);p->blocksize=8;
 assert(stm32_dmarecvsetup(&p->dev,arena,8)==0);
 assert(stm32_sendcmd(&p->dev,SD_CMD55,0x10000)==0);
 regs[9]=55;regs[10]=0;regs[0]=0;assert(stm32_recvshortcrc(&p->dev,SD_CMD55,&response)==0);
 assert(stm32_sendcmd(&p->dev,SD_ACMD51,0)==0);
 regs[9]=51;assert(stm32_recvshortcrc(&p->dev,SD_ACMD51,&response)==0);
 p->waitevents=SDIOWAIT_TRANSFERDONE;regs[0]=STM32_SDMMC_STA_DATAEND;regs[6]=0;regs[7]=8;regs[4]=STM32_SDMMC_DCTRL_DTDIR|STM32_SDMMC_DCTRL_8BYTES;
 assert(stm32_sdmmc_interrupt(0,NULL,p)==0);assert(stm32_scr_finish(&p->dev,key,0,scr)==0);
 assert(p->scrlease.state==SCR_LEASE_HANDOFF);
 switch(fault){
 case 0:regs[0]=STM32_SDMMC_STA_DPSMACT;break;
 case 1:regs[0]=STM32_SDMMC_STA_CPSMACT;break;
 case 2:regs[0]=STM32_SDMMC_STA_IDMATE;break;
 case 3:regs[5]=1;break;
 case 4:ccr=NVIC_CFGCON_DC;break;
 case 5:chipid=0;break;
 case 6:p->remaining=1;break;
 case 7:p->xfrmask=1;break;
 case 8:p->waitevents=1;break;
 case 9:p->waitmask=1;break;
 case 10:p->scrinitializing=true;break;
 case 11:p->scrlease.generation=UINT64_MAX;break;
 }
 arena=(uint8_t*)0x1234;memset(&next,0xa5,sizeof next);struct scr_lease_key sentinel=next;
 int result=stm32_sector_claim(&p->dev,1,4096,true,&arena,&next);
 assert(result==(fault==11?-EOVERFLOW:-EBUSY));
 assert(arena==(uint8_t*)0x1234 && memcmp(&next,&sentinel,sizeof next)==0);
 assert(p->sector_attempted && p->scrlease.state==SCR_LEASE_RETIRED && p->scrlease.cpu_destination==NULL);
 regs[0]=0;regs[5]=0;ccr=0;chipid=0x20030450;p->remaining=p->xfrmask=p->waitevents=p->waitmask=0;p->scrinitializing=false;
 assert(stm32_sector_claim(&p->dev,1,4096,true,&arena,&next)==-EBUSY);
 assert(p->scrlease.state==SCR_LEASE_RETIRED);
 }
 munmap(m,4096);puts("PASS 12 actual SCR handoff rejection causes: failed sector attempt retires permanently, preserves out/key, and cannot retry after condition clears");return 0;
}
