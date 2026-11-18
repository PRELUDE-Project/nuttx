#define main original_fixture_main
#include "${RECOVERY_ROOT}/nuttx/tools/hosttests/scr_sector/driver.c"
#undef main
static void stm32_eventtimeout(wdparm_t arg)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)arg;

#ifdef CONFIG_SD_SECTOR_READ_REVIEW
  /* A callback already dispatched before cancellation may arrive after
   * one-shot quarantine/retirement. It must not publish another wakeup.
   * This is not a hardware-stop or callback-join acknowledgement.
   */
  if (scr_closed(priv)) return;
#endif

  /* There is always race conditions with timer expirations. */

  DEBUGASSERT((priv->waitevents & SDIOWAIT_TIMEOUT) != 0 ||
              priv->wkupevent != 0);

  mcinfo("sta: %08" PRIx32 " enabled irq: %08" PRIx32 "\n",
         sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET),
         sdmmc_getreg32(priv, STM32_SDMMC_MASK_OFFSET));

  /* Is a data transfer complete event expected? */

  if ((priv->waitevents & SDIOWAIT_TIMEOUT) != 0)
    {
      /* Yes.. wake up any waiting threads */

#ifdef CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE
      stm32_endwait(priv, SDIOWAIT_TIMEOUT |
                    (priv->waitevents & SDIOWAIT_WRCOMPLETE));
#else
      stm32_endwait(priv, SDIOWAIT_TIMEOUT);
#endif
      mcerr("Timeout: remaining: %zu\n", priv->remaining);
    }
}


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
