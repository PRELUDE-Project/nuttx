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
