#define main original_fixture_main
#include "${RECOVERY_ROOT}/idma-write-attempt-audit/driver.c"
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


static int stm32_cancel(struct sdio_dev_s *dev)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  irqstate_t flags;

  flags = enter_critical_section();
#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW
  if (priv->scrlease.state != SCR_LEASE_IDLE)
    {
      /* Caller memory is detached; physical DMA/IRQ stop is NOT proved. */
      scr_seal(priv, -ECANCELED);
      wd_cancel(&priv->waitwdog);
      if (priv->waitevents != 0)
        {
          stm32_endwait(priv, SDIOWAIT_TRANSFERDONE | SDIOWAIT_ERROR);
        }
      leave_critical_section(flags);
      return -EINPROGRESS;
    }
#endif

  /* Disable all transfer- and event- related interrupts */

  stm32_configxfrints(priv, 0);
  stm32_configwaitints(priv, 0, 0, 0);

  /* Send a stop command to DPSM */

  sdmmc_putreg32(priv, STM32_SDMMC_CMD_CMDSTOP,
                 STM32_SDMMC_CMD_OFFSET);

  /* If this was a IDMA transfer, make sure that IDMA is stopped */

  sdmmc_putreg32(priv, 0, STM32_SDMMC_IDMACTRLR_OFFSET);

  /* Clearing pending interrupt status on all transfer- and event- related
   * interrupts
   */

  sdmmc_putreg32(priv, STM32_SDMMC_WAITALL_ICR, STM32_SDMMC_ICR_OFFSET);

  /* Cancel any watchdog timeout */

  wd_cancel(&priv->waitwdog);

#ifndef CONFIG_STM32H7_SDMMC_IDMA
  wd_cancel(&priv->fifowdog);
  priv->receivecnt = 0;
  priv->buffer = NULL;
#endif

  /* Mark no transfer in progress */

  priv->remaining = 0;
  leave_critical_section(flags);
  return OK;
}

int main(void){
 void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);
 struct stm32_dev_s*p=m;uint8_t*arena,out[512];struct scr_lease_key key;
 int inputs[]={0,-EBUSY,-EIO,-ETIMEDOUT,-ENOTSUP,-EINVAL};
 for(unsigned sector=0;sector<2;sector++)for(unsigned cause=0;cause<2;cause++)for(unsigned i=0;i<6;i++)for(unsigned active=0;active<4;active++){
 fresh(p);if(sector){key=begin(p);setup(p);}else{assert(stm32_scr_claim(&p->dev,&arena,&key)==0);p->blocksize=8;assert(stm32_dmarecvsetup(&p->dev,arena,8)==0);}
 p->waitevents=SDIOWAIT_TIMEOUT|SDIOWAIT_ERROR;regs[0]=(active&1?STM32_SDMMC_STA_DPSMACT:0)|(active&2?STM32_SDMMC_STA_CPSMACT:0);idma_write_attempts=stop_write_attempts=0;
 if(cause==0)stm32_eventtimeout((wdparm_t)p);
 assert(stm32_cancel(&p->dev)==-EINPROGRESS);
 int saved=cause==0?-ETIMEDOUT:-ECANCELED;
 assert(p->scrlease.result==saved && p->scrlease.state==SCR_LEASE_QUARANTINED);
 memset(out,0xa5,sizeof out);unsigned notes=notifications;
 int actual=sector?stm32_sector_finish(&p->dev,key,inputs[i],out):stm32_scr_finish(&p->dev,key,inputs[i],(uint32_t*)out);
 int expected=sector||inputs[i]==0||inputs[i]==-EBUSY?saved:inputs[i];
 assert(actual==expected && p->scrlease.result==saved && notifications==notes);
 for(unsigned j=0;j<sizeof out;j++)assert(out[j]==0xa5);
 assert(regs[5]==1);assert(idma_write_attempts==0&&stop_write_attempts==0);
 printf("sector=%u timeout_before_cancel=%u input=%d return=%d stored=%d\n",sector,cause==0,inputs[i],actual,saved);
 }
 puts("PASS 96 actual timeout/cancel/finish cases; zero IDMACTRLR and CMDSTOP write attempts regardless of active bits; no physical stop proof");
 munmap(m,4096);return 0;
}
