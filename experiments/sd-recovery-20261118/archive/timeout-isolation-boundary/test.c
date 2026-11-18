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
