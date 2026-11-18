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
 struct stm32_dev_s*p=m;uint8_t out[512],*arena;struct scr_lease_key key;
 unsigned ids[]={0x20010450,0x20030450,0x20070450,0x10030450,0x00000450,0x20030451,0xffffffff};unsigned count=0;
 for(unsigned sector=0;sector<2;sector++)for(unsigned id=0;id<7;id++)for(unsigned cache=0;cache<2;cache++){
 fresh(p);
 if(sector){key=begin(p);setup(p);command(p);terminal(p,0);}
 else{assert(stm32_scr_claim(&p->dev,&arena,&key)==0);p->blocksize=8;assert(stm32_dmarecvsetup(&p->dev,arena,8)==0);uint32_t r;assert(stm32_sendcmd(&p->dev,SD_CMD55,0x10000)==0);regs[9]=55;regs[10]=0;regs[0]=0;assert(stm32_recvshortcrc(&p->dev,SD_CMD55,&r)==0);assert(stm32_sendcmd(&p->dev,SD_ACMD51,0)==0);regs[9]=51;assert(stm32_recvshortcrc(&p->dev,SD_ACMD51,&r)==0);p->waitevents=SDIOWAIT_TRANSFERDONE;regs[0]=STM32_SDMMC_STA_DATAEND;regs[6]=0;regs[7]=8;regs[4]=STM32_SDMMC_DCTRL_DTDIR|STM32_SDMMC_DCTRL_8BYTES;assert(stm32_sdmmc_interrupt(0,NULL,p)==0);}
 memset(p->scrlease.arena,0x5a,512);memset(out,0xa5,512);chipid=ids[id];ccr=cache?NVIC_CFGCON_DC:0;unsigned prior=barriers;
 int r=sector?stm32_sector_finish(&p->dev,key,0,out):stm32_scr_finish(&p->dev,key,0,(uint32_t*)out);
 if(id<3&&!cache){assert(r==0);for(unsigned j=0;j<(sector?512u:8u);j++)assert(out[j]==0x5a);for(unsigned j=sector?512u:8u;j<512;j++)assert(out[j]==0xa5);assert(p->scrlease.state==(sector?SCR_LEASE_RETIRED:SCR_LEASE_HANDOFF));}
 else{assert(r==-EAGAIN&&p->scrlease.state==SCR_LEASE_QUARANTINED);for(unsigned j=0;j<512;j++)assert(out[j]==0xa5);assert(regs[5]==1&&barriers==prior);chipid=0x20030450;ccr=0;r=sector?stm32_sector_finish(&p->dev,key,0,out):stm32_scr_finish(&p->dev,key,0,(uint32_t*)out);assert(r==-EAGAIN);for(unsigned j=0;j<512;j++)assert(out[j]==0xa5);}
 count++;
 }
 printf("PASS %u post-IRQ platform rechecks, valid revisions/cache-off copy exact size; invalid conditions quarantine before IDMA disable and cannot recover by restoring inputs. Host only.\n",count);munmap(m,4096);return 0;
}
