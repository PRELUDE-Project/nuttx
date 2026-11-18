#define _GNU_SOURCE
#include <sys/mman.h>

#include <nuttx/scr_lease_review.h>
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define OK 0
#define STM32_DBGMCU_IDC 1
#define DBGMCU_IDCODE_REVID_SHIFT 16
#define DBGMCU_IDCODE_DEVID_MASK 0xfff
#define STM32_IDCODE_DEVID 0x450
#define STM32_SDMMC_DCTRL_DTMODE_MASK (3u<<2)
#define STM32_SDMMC_DCTRL_DTMODE_BLOCK 0
#define STM32_SDMMC_DCTRL_DBLOCKSIZE_MASK (15u<<4)
#define STM32_SDMMC_DCTRL_8BYTES (3u<<4)
#define STM32_SDMMC_ARG_OFFSET 11
#define SDMMC_CMDTIMEOUT 3
#define SDMMC_LONGTIMEOUT 4
#define NVIC_CFGCON 0
#define NVIC_CFGCON_DC 1
#define STM32_SDMMC_IDMACTRLR_OFFSET 5
#define STM32_SDMMC_DCOUNT_OFFSET 6
#define STM32_SDMMC_DLEN_OFFSET 7
#define STM32_SDMMC_IDMABASE0R_OFFSET 8
#define STM32_SDMMC_RESPCMD_OFFSET 9
#define STM32_SDMMC_RESP1_OFFSET 10
#define STM32_SDMMC_IDMACTRLR_IDMAEN 1
#define STM32_SDMMC_IDMACTRLR_IDMABMODE 2
#define STM32_SDMMC_DCTRL_DTDIR 2
#define STM32_SDMMC_STA_IDMATE (1u<<27)
#define STM32_SDMMC_STA_DABORT (1u<<11)
#define STM32_SDMMC_STA_DPSMACT (1u<<12)
#define STM32_SDMMC_STA_CPSMACT (1u<<13)
#define STM32_SDMMC_STA_CTIMEOUT (1u<<14)
#define STM32_SDMMC_STA_CCRCFAIL (1u<<15)
#define SCR_HW_ERRORS (STM32_SDMMC_STA_IDMATE|STM32_SDMMC_STA_DABORT|STM32_SDMMC_STA_DCRCFAIL|STM32_SDMMC_STA_DTIMEOUT|STM32_SDMMC_STA_RXOVERR|STM32_SDMMC_STA_TXUNDERR)
#define SCR_HW_ACTIVE (STM32_SDMMC_STA_DPSMACT|STM32_SDMMC_STA_CPSMACT)
#define STM32_SDMMC_RESPDONE_ICR ((1u<<14)|(1u<<15)|(1u<<7))
#define STM32_SDMMC_CMDDONE_ICR (1u<<8)
#define STM32_SDMMC_RESPCMD_MASK 63
#define MMCSD_CMDIDX_MASK 63
#define MMCSD_CMDIDX_SHIFT 0
static uintptr_t regs[16];static unsigned ccr,barriers;static bool fail_disable;static unsigned late_error;
static uint32_t chipid=0x20030450;
static uint32_t getreg32(int r){return r==1?chipid:ccr;}
#define UP_DSB() do {barriers++;if(late_error)regs[0]|=STM32_SDMMC_STA_IDMATE;} while(0)

#define DEBUGASSERT assert
#define mcerr(...) ((void)0)
#define mcinfo(...) ((void)0)
#define FIFO_SIZE_IN_BYTES 64
#define SDIOWAIT_TRANSFERDONE 1
#define SDIOWAIT_ERROR 2
#define SDIOWAIT_TIMEOUT 4
#define SDIOWAIT_RESPONSEDONE 8
#define SDIOWAIT_CMDDONE 16
#define STM32_SDMMC_STA_OFFSET 0
#define STM32_SDMMC_MASK_OFFSET 1
#define STM32_SDMMC_ICR_OFFSET 2
#define STM32_SDMMC_CMD_OFFSET 3
#define STM32_SDMMC_DCTRL_OFFSET 4
#define STM32_SDMMC_STA_RXFIFOHF (1u<<0)
#define STM32_SDMMC_STA_TXFIFOHE (1u<<1)
#define STM32_SDMMC_STA_DATAEND (1u<<2)
#define STM32_SDMMC_STA_DCRCFAIL (1u<<3)
#define STM32_SDMMC_STA_DTIMEOUT (1u<<4)
#define STM32_SDMMC_STA_RXOVERR (1u<<5)
#define STM32_SDMMC_STA_TXUNDERR (1u<<6)
#define STM32_SDMMC_RESPDONE_STA ((1u<<7)|(1u<<14)|(1u<<15))
#define STM32_SDMMC_CMDDONE_STA (1u<<8)
#define STM32_SDMMC_MASK_TXFIFOHEIE STM32_SDMMC_STA_TXFIFOHE
#define STM32_SDMMC_CMD_CMDSTOP 1
#define STM32_SDMMC_CMD_CPSMEN 2
#define STM32_SDMMC_DCTRL_DTEN 1
#define STM32_SDMMC_XFRDONE_ICR (0x7cu)
#define SAMPLENDX_END_TRANSFER 0
#define stm32_sample(p,n) ((void)0)
#define WDOG_ISACTIVE(w) (*(w))
typedef unsigned sdio_eventset_t;typedef unsigned irqstate_t;typedef uintptr_t wdparm_t;
struct sdio_dev_s {int placeholder;};
static uintptr_t current_owner=1;
static bool up_interrupt_context(void){return false;}
static uintptr_t nxsched_gettid(void){return current_owner;}
struct stm32_dev_s{struct sdio_dev_s dev;unsigned scr_phase;bool scr_completed;uint32_t scr_terminal_sta_before,scr_terminal_dcount,scr_terminal_idma,scr_terminal_sta_after;bool scrinitializing,onebit;uintptr_t scrinitowner;struct scr_lease_review scrlease;struct scr_lease_key scrkey;unsigned xfrmask,waitmask,waitevents;size_t remaining;unsigned receivecnt,blocksize;uint32_t *buffer;uint8_t rxfifo[64],sdmmc_rxbuffer[64];bool fifowdog,waitwdog,unaligned_rx;unsigned wkupevent;int waitsem;};
static unsigned depth,notifications,last_event,fifo,caches;static uint32_t status;
static unsigned enter_critical_section(void){return depth++;}
static void leave_critical_section(unsigned f){assert(depth==f+1);depth--;}

static unsigned count_reads;
static uintptr_t sdmmc_getreg32(struct stm32_dev_s*p,int off){if(off==6){count_reads++;assert((regs[0]&(STM32_SDMMC_STA_DPSMACT|STM32_SDMMC_STA_CPSMACT|STM32_SDMMC_STA_DTIMEOUT|STM32_SDMMC_STA_DCRCFAIL|STM32_SDMMC_STA_IDMATE|STM32_SDMMC_STA_DABORT))==0);assert(regs[0]&STM32_SDMMC_STA_DATAEND);}return off==1?p->xfrmask|p->waitmask:regs[off];}
static void sdmmc_putreg32(struct stm32_dev_s*p,uintptr_t v,int off){(void)p;if(off==2)regs[0]&=~v;else if(!(off==5&&fail_disable))regs[off]=v;}
static void sdmmc_modifyreg32(struct stm32_dev_s*p,int off,uint32_t a,uint32_t b){(void)p;(void)off;(void)a;(void)b;}
static void stm32_configxfrints(struct stm32_dev_s*p,unsigned mask){p->xfrmask=mask;}
static void stm32_configwaitints(struct stm32_dev_s*p,unsigned mask,unsigned ev,unsigned wake){p->waitmask=mask;p->waitevents=ev;p->wkupevent=wake;last_event=wake;}
static struct stm32_dev_s *observed;
static void nxsem_post(int*s){assert(observed->buffer==NULL && observed->xfrmask==0 && observed->waitmask==0);(*s)++;notifications++;}

static int wd_cancel(bool*w){*w=false;return 0;}
static int wd_start(bool*w,int delay,void(*f)(wdparm_t),wdparm_t a){(void)delay;(void)f;(void)a;*w=true;return 0;}
static void stm32_sdmmc_fifo_monitor(wdparm_t x){(void)x;}
static void stm32_recvfifo(struct stm32_dev_s*p){(void)p;fifo++;status&=~STM32_SDMMC_STA_RXFIFOHF;}
static void stm32_sendfifo(struct stm32_dev_s*p){(void)p;fifo++;status&=~STM32_SDMMC_STA_TXFIFOHE;}
static void up_invalidate_dcache(uintptr_t a,uintptr_t b){(void)a;(void)b;caches++;}

#include "../scr_containment/actual-command-constants.h"

#define CONFIG_SD_SCR_CONTAINMENT_REVIEW 1
#define STM32_AXISRAM_BASE 0x24000000u
#define STM32H7_SRAM_SIZE (512u*1024)
#define FAR
#define finfo(...) ((void)0)
#define ferr(...) ((void)0)
#define SDMMC_DTIMER_DATATIMEOUT_MS 100
#define STM32_SDMMC_CLK 1000000
#define STM32_SDMMC_CLKCR_OFFSET 12
#define STM32_SDMMC_DTIMER_OFFSET 13
#define STM32_SDMMC_CLKCR_CLKDIV_MASK 1023
#define STM32_SDMMC_CLKCR_CLKDIV_SHIFT 0
#define STM32_SDMMC_DCTRL_BOOTACKEN 256
#define STM32_SDMMC_DCTRL_DBLOCKSIZE_SHIFT 4
#define SAMPLENDX_BEFORE_SETUP 0
#define SAMPLENDX_AFTER_SETUP 1
#define STM32_SDMMC_RECV_MASK 0x7d
#define STM32_SDMMC_DMARECV_MASK 0x7c
#define STM32_SDMMC_XFRDONE_MASK 0x7c
#define STM32_SDMMC_CMDDONE_MASK 0x100
#define STM32_SDMMC_RESPDONE_MASK 0x80
#define STM32_SDMMC_WAITALL_ICR 0x1fc
#define MMCSD_SCR_DATADELAY 100
#define SDIO_CAPS_DMASUPPORTED 1
#define MSEC2TICK(x) (x)
#define stm32_sampleinit() ((void)0)
#define stm32_dumpsamples(p) ((void)0)
static unsigned stm32_log2(unsigned x){unsigned n=0;while(x>1){x>>=1;n++;}return n;}
static int nxsem_wait_uninterruptible(int*s){assert(*s>0);--*s;return 0;}
static bool scr_closed(struct stm32_dev_s *priv)
{
  return priv->scrlease.state == SCR_LEASE_QUARANTINED ||
         priv->scrlease.state == SCR_LEASE_RETIRED;
}
static bool scr_denied(struct stm32_dev_s *priv)
{
  return scr_closed(priv) || priv->scrinitializing ||
    (priv->scrlease.state == SCR_LEASE_ACTIVE &&
     (up_interrupt_context() || priv->scrlease.owner != (uintptr_t)nxsched_gettid()));
}
static void scr_close_producers(struct stm32_dev_s *priv)
{
  stm32_configxfrints(priv, 0);
  priv->waitmask = 0;
  sdmmc_putreg32(priv, 0, STM32_SDMMC_MASK_OFFSET);
#ifndef CONFIG_STM32H7_SDMMC_IDMA
  wd_cancel(&priv->fifowdog);
#endif
  priv->buffer = NULL;
  priv->remaining = 0;
  priv->receivecnt = 0;
  priv->unaligned_rx = false;
}
static void scr_seal(struct stm32_dev_s *priv, int error)
{
  irqstate_t f = enter_critical_section();
  if (priv->scrlease.state != SCR_LEASE_IDLE)
    {
      scr_lease_end(&priv->scrlease, priv->scrkey, error);
      scr_close_producers(priv);
    }
  leave_critical_section(f);
}
static bool scr_platform_qualified(void)
{
  uint32_t id = getreg32(STM32_DBGMCU_IDC);
  uint32_t revision = id >> DBGMCU_IDCODE_REVID_SHIFT;
  /* ES0392 Rev16: this candidate does not implement the Y/W AXI SRAM
   * read-corruption workaround. Unknown silicon is not qualified. */
  return (id & DBGMCU_IDCODE_DEVID_MASK) == STM32_IDCODE_DEVID &&
         (revision == 0x2001 || revision == 0x2003 || revision == 0x2007) &&
         (getreg32(NVIC_CFGCON) & NVIC_CFGCON_DC) == 0;
}
static bool scr_arena_qualified(uintptr_t address)
{
  return (address & 31u) == 0 && address >= STM32_AXISRAM_BASE &&
         address - STM32_AXISRAM_BASE <= STM32H7_SRAM_SIZE - 32u;
}
int stm32_scr_claim(struct sdio_dev_s *dev, uint8_t **arena,
                    struct scr_lease_key *key)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  irqstate_t f = enter_critical_section();
  int ret = -EBUSY;
  uint32_t sta = sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET);
  uint32_t idma = sdmmc_getreg32(priv, STM32_SDMMC_IDMACTRLR_OFFSET);
  if (!up_interrupt_context() && !priv->scrinitializing &&
      priv->remaining == 0 && priv->xfrmask == 0 &&
      priv->waitevents == 0 && priv->waitmask == 0 &&
      scr_platform_qualified() &&
      scr_arena_qualified((uintptr_t)priv->scrlease.arena) &&
      (sta & (SCR_HW_ERRORS | SCR_HW_ACTIVE)) == 0 &&
      (idma & STM32_SDMMC_IDMACTRLR_IDMAEN) == 0)
    ret = scr_lease_begin(&priv->scrlease, (uintptr_t)nxsched_gettid(),
                          priv->scrlease.arena, key);
  if (ret == 0)
    {
      priv->scrkey = *key;
      priv->scr_phase = 0;
      priv->scr_completed = false;
      sdmmc_putreg32(priv, STM32_SDMMC_STA_DATAEND, STM32_SDMMC_ICR_OFFSET);
      if ((sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET) &
           (SCR_HW_ERRORS | SCR_HW_ACTIVE | STM32_SDMMC_STA_DATAEND)) != 0)
        {
          scr_seal(priv, -EIO);
          leave_critical_section(f);
          return -EIO;
        }
      *arena = priv->scrlease.arena;
    }
  leave_critical_section(f);
  return ret;
}
int stm32_scr_finish(struct sdio_dev_s *dev, struct scr_lease_key key,
                     int result, uint32_t output[2])
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  irqstate_t flags = enter_critical_section();
  uint32_t sta;
  uint32_t idma;
  int ret = -ESTALE;
  if (!scr_lease_matches(&priv->scrlease, key) || up_interrupt_context() ||
      key.owner != (uintptr_t)nxsched_gettid() ||
      priv->scrlease.state == SCR_LEASE_IDLE)
    goto out;
  if (result < 0)
    {
      /* A sealed lease rejects later command admission with EBUSY. Keep
       * the cause (for example watchdog-arm failure), not that consequence.
       * Other operation errors still take precedence over a later cancel.
       */

      if (result == -EBUSY &&
          priv->scrlease.state == SCR_LEASE_QUARANTINED &&
          priv->scrlease.result < 0)
        {
          ret = priv->scrlease.result;
        }
      else
        {
          scr_seal(priv, result);
          ret = result;
        }

      goto out;
    }
  if (priv->scrlease.state != SCR_LEASE_ACTIVE)
    {
      ret = priv->scrlease.result;
      goto out;
    }
  sta = sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET);
  idma = sdmmc_getreg32(priv, STM32_SDMMC_IDMACTRLR_OFFSET);
  if (!output || !priv->scr_completed || priv->scr_phase != 4 ||
      !scr_platform_qualified() ||
      !scr_arena_qualified((uintptr_t)priv->scrlease.arena) ||
      (sta & (SCR_HW_ERRORS | SCR_HW_ACTIVE)) != 0 ||
      (sta & STM32_SDMMC_STA_DATAEND) == 0 ||
      sdmmc_getreg32(priv, STM32_SDMMC_DCOUNT_OFFSET) != 0 ||
      sdmmc_getreg32(priv, STM32_SDMMC_DLEN_OFFSET) != 8 ||
      sdmmc_getreg32(priv, STM32_SDMMC_IDMABASE0R_OFFSET) !=
        (uintptr_t)priv->scrlease.arena ||
      (idma & STM32_SDMMC_IDMACTRLR_IDMABMODE) != 0 ||
      (sdmmc_getreg32(priv, STM32_SDMMC_DCTRL_OFFSET) &
       (STM32_SDMMC_DCTRL_DTDIR | STM32_SDMMC_DCTRL_DTMODE_MASK |
        STM32_SDMMC_DCTRL_DBLOCKSIZE_MASK)) !=
       (STM32_SDMMC_DCTRL_DTDIR | STM32_SDMMC_DCTRL_DTMODE_BLOCK |
        STM32_SDMMC_DCTRL_8BYTES) || priv->xfrmask != 0 ||
      priv->waitmask != 0 || priv->buffer != NULL)
    {
      scr_seal(priv, -EAGAIN);
      ret = -EAGAIN;
      goto out;
    }
  /* RM0433 single-buffer DATAEND denotes IDMA+data completion.
   * IDMAEN is writable only after observed DPSM inactive. */
  sdmmc_putreg32(priv, 0, STM32_SDMMC_IDMACTRLR_OFFSET);
  UP_DSB();
  sta = sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET);
  if (sdmmc_getreg32(priv, STM32_SDMMC_IDMACTRLR_OFFSET) != 0 ||
      (sta & (SCR_HW_ERRORS | SCR_HW_ACTIVE)) != 0 ||
      (sta & STM32_SDMMC_STA_DATAEND) == 0 ||
      sdmmc_getreg32(priv, STM32_SDMMC_DCOUNT_OFFSET) != 0)
    {
      scr_seal(priv, -EIO);
      ret = -EIO;
      goto out;
    }
  UP_DSB();
  sdmmc_putreg32(priv, STM32_SDMMC_STA_DATAEND, STM32_SDMMC_ICR_OFFSET);
  sta = sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET);
  if ((sta & (SCR_HW_ERRORS | SCR_HW_ACTIVE | STM32_SDMMC_STA_DATAEND)) != 0)
    {
      scr_seal(priv, -EIO);
      ret = -EIO;
      goto out;
    }
  memcpy(output, priv->scrlease.arena, 8);
  priv->scrlease.cpu_destination = NULL;
  priv->scrlease.owner = 0;
  priv->scrlease.result = 0;
  priv->scrlease.state = SCR_LEASE_IDLE;
  priv->scr_completed = false;
  priv->scr_phase = 0;
  ret = 0;
out:
  leave_critical_section(flags);
  return ret;
}
int stm32_scr_stop_status(struct sdio_dev_s *dev, struct scr_lease_key key)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  irqstate_t flags = enter_critical_section();
  int result = -ESTALE;
  if (scr_lease_matches(&priv->scrlease, key) &&
      key.owner == (uintptr_t)nxsched_gettid() && !up_interrupt_context())
    {
      /* Observation only. Never grants recovery, copy or reuse. */
      result = scr_closed(priv) ? -EINPROGRESS : -EBUSY;
    }
  leave_critical_section(flags);
  return result;
}
static void stm32_datadisable(struct stm32_dev_s *priv)
{
  uint32_t regval;

  /* Disable the IDMA */

  sdmmc_putreg32(priv, 0, STM32_SDMMC_IDMACTRLR_OFFSET);

  /* Disable the data path */

  /* Reset DTIMER */

  sdmmc_putreg32(priv, UINT32_MAX, STM32_SDMMC_DTIMER_OFFSET);

  /* Reset DLEN */

  sdmmc_putreg32(priv,  0, STM32_SDMMC_DLEN_OFFSET);

  /* Reset DCTRL DTEN, DTDIR, DTMODE, and DBLOCKSIZE fields */

  regval  = sdmmc_getreg32(priv, STM32_SDMMC_DCTRL_OFFSET);
  regval &= ~(STM32_SDMMC_DCTRL_DTEN  | STM32_SDMMC_DCTRL_DBLOCKSIZE_MASK |
              STM32_SDMMC_DCTRL_DTDIR | STM32_SDMMC_DCTRL_DTMODE_MASK);
  sdmmc_putreg32(priv, regval, STM32_SDMMC_DCTRL_OFFSET);
}
static void stm32_dataconfig(struct stm32_dev_s *priv, uint32_t timeout,
                             uint32_t dlen, bool receive)
{
  uint32_t clkdiv;
  uint32_t regval;
  uint32_t dctrl;
  uint32_t sdio_clk = STM32_SDMMC_CLK;

  DEBUGASSERT((sdmmc_getreg32(priv, STM32_SDMMC_IDMACTRLR_OFFSET) &
               STM32_SDMMC_IDMACTRLR_IDMAEN) == 0);
  DEBUGASSERT((sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET) &
               STM32_SDMMC_STA_DPSMACT) == 0);

  /* Configure DCTRL DTDIR, DTMODE, and DBLOCKSIZE fields.
   * Note: the DTEN is not used, DPSM, and CPSM are used.
   */

  dctrl  =  sdmmc_getreg32(priv, STM32_SDMMC_DCTRL_OFFSET);
  dctrl  &= ~(STM32_SDMMC_DCTRL_DTDIR | STM32_SDMMC_DCTRL_DTMODE_MASK |
              STM32_SDMMC_DCTRL_DBLOCKSIZE_MASK | STM32_SDMMC_DCTRL_DTEN |
              STM32_SDMMC_DCTRL_BOOTACKEN);

  dctrl  &= (STM32_SDMMC_DCTRL_DTDIR | STM32_SDMMC_DCTRL_DTMODE_MASK |
             STM32_SDMMC_DCTRL_DBLOCKSIZE_MASK);

  /* Configure the data direction */

  if (receive)
    {
      dctrl |= STM32_SDMMC_DCTRL_DTDIR;
    }

  /* Set SDIO_MODE */

#if defined(HAVE_SDMMC_SDIO_MODE)
  if (priv->sdiomode == true)
    {
      dctrl |= STM32_SDMMC_DCTRL_SDIOEN;
    }
#endif

  dctrl |= STM32_SDMMC_DCTRL_DTMODE_BLOCK;

  /* if dlen > priv->blocksize we assume that this is a multi-block transfer
   * and that the len is multiple of priv->blocksize.
   */

  if (dlen > priv->blocksize)
    {
      DEBUGASSERT((dlen % priv->blocksize) == 0);

#if defined(CONFIG_STM32H7_SDMMC_IDMA)
      /* If this is an unaligned receive, then receive one block at a
       * time to the internal buffer
       */

      if (priv->unaligned_rx)
        {
          DEBUGASSERT(priv->blocksize <= sizeof(priv->sdmmc_rxbuffer));
          dlen = priv->blocksize;
        }
#endif
    }

  dctrl |= stm32_log2(priv->blocksize) << STM32_SDMMC_DCTRL_DBLOCKSIZE_SHIFT;

  /* Enable data path */

  /* Set DTIMER
   *
   * Enable data path using a timeout scaled to the SD_CLOCK (the card
   * clock).
   */

  regval = sdmmc_getreg32(priv, STM32_SDMMC_CLKCR_OFFSET);
  clkdiv = (regval & STM32_SDMMC_CLKCR_CLKDIV_MASK) >>
            STM32_SDMMC_CLKCR_CLKDIV_SHIFT;

  /* CLKDIV_ of 0x000: is Bypass */

  if (clkdiv != 0)
    {
      sdio_clk = sdio_clk / (2 * clkdiv);
    }

  /*  Convert Timeout in Ms to SD_CLK counts */

  timeout  = timeout * (sdio_clk / 1000);

  sdmmc_putreg32(priv, timeout, STM32_SDMMC_DTIMER_OFFSET);

  /* Set DLEN */

  sdmmc_putreg32(priv, dlen, STM32_SDMMC_DLEN_OFFSET);

  /* Set DCTRL */

  sdmmc_putreg32(priv, dctrl, STM32_SDMMC_DCTRL_OFFSET);
}
static void stm32_blocksetup(struct sdio_dev_s *dev,
                             unsigned int blocksize, unsigned int nblocks)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  irqstate_t scrflags = enter_critical_section();
  if (scr_denied(priv) || (priv->scrlease.state == SCR_LEASE_ACTIVE && (priv->scr_phase != 0 || priv->scr_completed || blocksize != 8 || nblocks != 1)))
    {
      leave_critical_section(scrflags);
      return ;
    }


  priv->blocksize = blocksize;

  leave_critical_section(scrflags);
}
static void stm32_endwait(struct stm32_dev_s *priv,
                          sdio_eventset_t wkupevent)
{
  if (priv->scrlease.state != SCR_LEASE_IDLE)
    {
      if (wkupevent == SDIOWAIT_TRANSFERDONE && priv->scr_completed)
        scr_close_producers(priv);
      else
        scr_seal(priv, (wkupevent & SDIOWAIT_TIMEOUT) ? -ETIMEDOUT : -EIO);
    }

  /* Cancel the watchdog timeout */

  wd_cancel(&priv->waitwdog);

  /* Disable event-related interrupts */

  stm32_configwaitints(priv, 0, 0, wkupevent);

  /* Wake up the waiting thread */

  nxsem_post(&priv->waitsem);
}
static void stm32_eventtimeout(wdparm_t arg)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)arg;

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
static void stm32_endtransfer(struct stm32_dev_s *priv,
                              sdio_eventset_t wkupevent)
{
  irqstate_t flags = enter_critical_section();

  if (priv->scrlease.state != SCR_LEASE_IDLE)
    {
      /* Preserve hardware terminal evidence; no fabricated stop sequence. */
      if (wkupevent == SDIOWAIT_TRANSFERDONE && priv->scr_completed)
        scr_close_producers(priv);
      else
        scr_seal(priv, (wkupevent & SDIOWAIT_TIMEOUT) ? -ETIMEDOUT : -EIO);
      if ((priv->waitevents & wkupevent) != 0)
        {
          stm32_endwait(priv, wkupevent);
        }
      leave_critical_section(flags);
      return;
    }

  /* Disable all transfer related interrupts */

  stm32_configxfrints(priv, 0);

#ifndef CONFIG_STM32H7_SDMMC_IDMA
  wd_cancel(&priv->fifowdog);
#endif

  /* If there were errors, send a stop command to DPSM */

  if ((wkupevent & (~SDIOWAIT_TRANSFERDONE)) != 0)
    {
      sdmmc_putreg32(priv, STM32_SDMMC_CMD_CMDSTOP,
                     STM32_SDMMC_CMD_OFFSET);
    }
  else
    {
      sdmmc_modifyreg32(priv, STM32_SDMMC_CMD_OFFSET,
                        STM32_SDMMC_CMD_CPSMEN, 0);
    }

  /* Clearing pending interrupt status on all transfer related interrupts */

  sdmmc_putreg32(priv, STM32_SDMMC_XFRDONE_ICR, STM32_SDMMC_ICR_OFFSET);

  /* DMA debug instrumentation */

  stm32_sample(priv, SAMPLENDX_END_TRANSFER);

  /* Mark the transfer finished */

  priv->remaining = 0;

#ifndef CONFIG_STM32H7_SDMMC_IDMA
  priv->receivecnt = 0;
  priv->buffer = NULL;
#endif

  /* Is a thread wait for these data transfer complete events? */

  if ((priv->waitevents & wkupevent) != 0)
    {
      /* Yes.. wake up any waiting threads */

      stm32_endwait(priv, wkupevent);
    }

  leave_critical_section(flags);
}
static int stm32_sendcmd(struct sdio_dev_s *dev, uint32_t cmd,
                         uint32_t arg)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  irqstate_t scrflags = enter_critical_section();
  if (scr_denied(priv))
    {
      leave_critical_section(scrflags);
      return -EBUSY;
    }

  if (priv->scrlease.state == SCR_LEASE_ACTIVE)
    {
      unsigned index = (cmd & MMCSD_CMDIDX_MASK) >> MMCSD_CMDIDX_SHIFT;
      if (priv->scr_completed ||
          !((cmd == MMCSD_CMD16 && arg == 8 && priv->scr_phase == 0) ||
            (cmd == SD_CMD55 && (arg & 0xffff) == 0 && arg != 0 &&
             priv->scr_phase == 0) ||
            (cmd == SD_ACMD51 && arg == 0 && priv->scr_phase == 2)))
        {
          leave_critical_section(scrflags);
          return -EBUSY;
        }
      if (index == 55) priv->scr_phase = 1;
      if (index == 51) priv->scr_phase = 3;
    }

  uint32_t regval;
  uint32_t cmdidx;

  /* Clear CMDINDEX, CMDTRANS, WAITRESP, WAITINT, WAITPEND, and CPSMEN bits */

  regval = sdmmc_getreg32(priv, STM32_SDMMC_CMD_OFFSET);

  /* Make sure the register is clear */

  if (regval & STM32_SDMMC_CMD_CPSMEN)
    {
      sdmmc_putreg32(priv, 0, STM32_SDMMC_CMD_OFFSET);
    }

  /* Set the SDIO Argument value */

  sdmmc_putreg32(priv, arg, STM32_SDMMC_ARG_OFFSET);

  /* Clear CMDINDEX, WAITRESP, WAITINT, WAITPEND, and CPSMEN bits */

  regval &= ~(STM32_SDMMC_CMD_CMDINDEX_MASK | STM32_SDMMC_CMD_WAITRESP_MASK |
              STM32_SDMMC_CMD_CMDTRANS      |  STM32_SDMMC_CMD_CMDSTOP      |
              STM32_SDMMC_CMD_WAITRESP_MASK | STM32_SDMMC_CMD_WAITINT       |
              STM32_SDMMC_CMD_WAITPEND      | STM32_SDMMC_CMD_CPSMEN        |
              STM32_SDMMC_CMD_DTHOLD        | STM32_SDMMC_CMD_BOOTMODE      |
              STM32_SDMMC_CMD_BOOTEN        | STM32_SDMMC_CMD_SUSPEND);

  /* Set WAITRESP bits */

  switch (cmd & MMCSD_RESPONSE_MASK)
    {
      case MMCSD_NO_RESPONSE:
        regval |= STM32_SDMMC_CMD_NORESPONSE;
        break;

      case MMCSD_R1_RESPONSE:
      case MMCSD_R1B_RESPONSE:
      case MMCSD_R3_RESPONSE:
      case MMCSD_R4_RESPONSE:
      case MMCSD_R5_RESPONSE:
      case MMCSD_R6_RESPONSE:
      case MMCSD_R7_RESPONSE:
        regval |= STM32_SDMMC_CMD_SHORTRESPONSE;
        break;

      case MMCSD_R2_RESPONSE:
        regval |= STM32_SDMMC_CMD_LONGRESPONSE;
        break;
    }

  /* Set CPSMEN and the command index */

  cmdidx  = (cmd & MMCSD_CMDIDX_MASK) >> MMCSD_CMDIDX_SHIFT;
  regval |= cmdidx | STM32_SDMMC_CMD_CPSMEN;

  switch (cmd & MMCSD_DATAXFR_MASK)
    {
    case MMCSD_RDDATAXFR: /* Read block transfer */
    case MMCSD_WRDATAXFR: /* Write block transfer */
    case MMCSD_RDSTREAM:  /* MMC Read stream */
    case MMCSD_WRSTREAM:  /* MMC Write stream */
        regval |= STM32_SDMMC_CMD_CMDTRANS;
        break;

    case MMCSD_NODATAXFR:
    default:
      if ((cmd & MMCSD_STOPXFR) != 0)
        {
          regval |= STM32_SDMMC_CMD_CMDSTOP;
        }
      break;
    }

  /* Clear interrupts */

  sdmmc_putreg32(priv, STM32_SDMMC_CMDDONE_ICR | STM32_SDMMC_RESPDONE_ICR,
                 STM32_SDMMC_ICR_OFFSET);

  mcinfo("cmd: %08" PRIx32 " arg: %08" PRIx32 " regval: %08" PRIx32
         " enabled irq: %08" PRIx32 "\n",
         cmd, arg, regval, sdmmc_getreg32(priv, STM32_SDMMC_MASK_OFFSET));

  /* Write the SDIO CMD */

  sdmmc_putreg32(priv, regval, STM32_SDMMC_CMD_OFFSET);
do { leave_critical_section(scrflags); return OK; } while (0);
}
static int stm32_waitresponse(struct sdio_dev_s *dev, uint32_t cmd)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  int32_t timeout;
  uint32_t events;
  irqstate_t scrflags = enter_critical_section();
  bool denied = scr_denied(priv);
  leave_critical_section(scrflags);
  if (denied) return -EBUSY;

  switch (cmd & MMCSD_RESPONSE_MASK)
    {
      case MMCSD_NO_RESPONSE:
        events  = STM32_SDMMC_CMDDONE_STA;
        timeout = SDMMC_CMDTIMEOUT;
        break;

      case MMCSD_R1_RESPONSE:
      case MMCSD_R1B_RESPONSE:
      case MMCSD_R2_RESPONSE:
      case MMCSD_R4_RESPONSE:
      case MMCSD_R5_RESPONSE:
      case MMCSD_R6_RESPONSE:
        events  = STM32_SDMMC_RESPDONE_STA;
        timeout = SDMMC_LONGTIMEOUT;
        break;

      case MMCSD_R3_RESPONSE:
      case MMCSD_R7_RESPONSE:
        events  = STM32_SDMMC_RESPDONE_STA;
        timeout = SDMMC_CMDTIMEOUT;
        break;

      default:
        return -EINVAL;
    }

  /* Then wait for the response (or timeout) */

  while ((sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET) & events) == 0)
    {
      scrflags = enter_critical_section();
      denied = scr_denied(priv);
      leave_critical_section(scrflags);
      if (denied) return -EBUSY;
      if (--timeout <= 0)
        {
          mcerr("ERROR: Timeout cmd: %08" PRIx32 " events: %08" PRIx32
                " STA: %08" PRIx32 "\n",
                cmd, events, sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET));

          return -ETIMEDOUT;
        }
    }

  scrflags = enter_critical_section();
  if (scr_denied(priv))
    {
      leave_critical_section(scrflags);
      return -EBUSY;
    }
  if (priv->scrlease.state == SCR_LEASE_ACTIVE)
    {
      uint32_t response = sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET);
      int response_result = (response & STM32_SDMMC_STA_CTIMEOUT) ?
        -ETIMEDOUT : (response & STM32_SDMMC_STA_CCRCFAIL) ? -EIO : OK;
      /* Preserve response evidence for the owner's R1 collection. */
      leave_critical_section(scrflags);
      return response_result;
    }
  sdmmc_putreg32(priv, events, STM32_SDMMC_ICR_OFFSET);
  leave_critical_section(scrflags);
  return OK;
}
static int stm32_recvshortcrc(struct sdio_dev_s *dev, uint32_t cmd,
                              uint32_t *rshort)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  irqstate_t scrflags = enter_critical_section();
  if (scr_denied(priv))
    {
      leave_critical_section(scrflags);
      return -EBUSY;
    }

#if defined(CONFIG_DEBUG_FEATURES)
  uint32_t respcmd;
#endif
  uint32_t regval;
  int ret = OK;

  /* R1  Command response (48-bit)
   *     47        0               Start bit
   *     46        0               Transmission bit (0=from card)
   *     45:40     bit5   - bit0   Command index (0-63)
   *     39:8      bit31  - bit0   32-bit card status
   *     7:1       bit6   - bit0   CRC7
   *     0         1               End bit
   *
   * R1b Identical to R1 with the additional busy signaling via the data
   *     line.
   *
   * R6  Published RCA Response (48-bit, SD card only)
   *     47        0               Start bit
   *     46        0               Transmission bit (0=from card)
   *     45:40     bit5   - bit0   Command index (0-63)
   *     39:8      bit31  - bit0   32-bit Argument Field, consisting of:
   *                               [31:16] New published RCA of card
   *                               [15:0]  Card status bits {23,22,19,12:0}
   *     7:1       bit6   - bit0   CRC7
   *     0         1               End bit
   */

#if defined(CONFIG_DEBUG_FEATURES)
  if (!rshort)
    {
      mcerr("ERROR: rshort=NULL\n");
      ret = -EINVAL;
    }

  /* Check that this is the correct response to this command */

  else if ((cmd & MMCSD_RESPONSE_MASK) != MMCSD_R1_RESPONSE &&
           (cmd & MMCSD_RESPONSE_MASK) != MMCSD_R1B_RESPONSE &&
           (cmd & MMCSD_RESPONSE_MASK) != MMCSD_R5_RESPONSE &&
           (cmd & MMCSD_RESPONSE_MASK) != MMCSD_R6_RESPONSE)
    {
      mcerr("ERROR: Wrong response CMD=%08" PRIx32 "\n", cmd);
      ret = -EINVAL;
    }
  else
#endif
    {
      /* Check if a timeout or CRC error occurred */

      regval = sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET);
      if ((regval & STM32_SDMMC_STA_CTIMEOUT) != 0)
        {
          mcerr("ERROR: Command timeout: %08" PRIx32 "\n", regval);
          ret = -ETIMEDOUT;
        }
      else if ((regval & STM32_SDMMC_STA_CCRCFAIL) != 0)
        {
          mcerr("ERROR: CRC failure: %08" PRIx32 "\n", regval);
          ret = -EIO;
        }
#if defined(CONFIG_DEBUG_FEATURES)
    else
        {
          /* Check response received is of desired command */

          respcmd = sdmmc_getreg32(priv, STM32_SDMMC_RESPCMD_OFFSET);
          if ((uint8_t)(respcmd & STM32_SDMMC_RESPCMD_MASK) !=
              (cmd & MMCSD_CMDIDX_MASK))
            {
              mcerr("ERROR: RESCMD=%02" PRIx32 " CMD=%08" PRIx32
                    "\n", respcmd, cmd);
              ret = -EINVAL;
            }
        }
#endif
    }

  /* Clear all pending message completion events and return the R1/R6
   * response.
   */

  sdmmc_putreg32(priv, STM32_SDMMC_RESPDONE_ICR | STM32_SDMMC_CMDDONE_ICR,
                 STM32_SDMMC_ICR_OFFSET);
  *rshort = sdmmc_getreg32(priv, STM32_SDMMC_RESP1_OFFSET);
  if (ret == OK && priv->scrlease.state == SCR_LEASE_ACTIVE)
    {
      unsigned index = (cmd & MMCSD_CMDIDX_MASK) >> MMCSD_CMDIDX_SHIFT;
      unsigned received = sdmmc_getreg32(priv, STM32_SDMMC_RESPCMD_OFFSET) &
                          STM32_SDMMC_RESPCMD_MASK;
      if (index != received) ret = -EIO;
      else if (index == 55 && priv->scr_phase == 1) priv->scr_phase = 2;
      else if (index == 51 && priv->scr_phase == 3) priv->scr_phase = 4;
      else if (cmd != MMCSD_CMD16 || priv->scr_phase != 0) ret = -EIO;
    }

do { leave_critical_section(scrflags); return ret; } while (0);
}
static void stm32_waitenable(struct sdio_dev_s *dev,
                             sdio_eventset_t eventset, uint32_t timeout)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  irqstate_t scrflags = enter_critical_section();
  if (scr_denied(priv) || (priv->scrlease.state == SCR_LEASE_ACTIVE && (priv->scr_phase != 0 || priv->scr_completed)))
    {
      leave_critical_section(scrflags);
      return ;
    }

  uint32_t waitmask = 0;

  DEBUGASSERT(priv != NULL);

  /* Disable event-related interrupts */

  stm32_configwaitints(priv, 0, 0, 0);

  /* Select the interrupt mask that will give us the appropriate wakeup
   * interrupts.
   */

#if defined(CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE)
  if ((eventset & SDIOWAIT_WRCOMPLETE) != 0)
    {
      /* Read pin to see if ready (true) skip timeout and the pin IRQ */

      if (stm32_gpioread(priv->d0_gpio))
        {
          eventset &= ~(SDIOWAIT_TIMEOUT | SDIOWAIT_WRCOMPLETE);
        }
    }
  else
#endif
    {
      if ((eventset & SDIOWAIT_CMDDONE) != 0)
        {
          waitmask |= STM32_SDMMC_CMDDONE_MASK;
        }

      if ((eventset & SDIOWAIT_RESPONSEDONE) != 0)
        {
          waitmask |= STM32_SDMMC_RESPDONE_MASK;
        }

      if ((eventset & SDIOWAIT_TRANSFERDONE) != 0)
        {
          waitmask |= STM32_SDMMC_XFRDONE_MASK;
        }

      /* Enable event-related interrupts */

      sdmmc_putreg32(priv, STM32_SDMMC_WAITALL_ICR, STM32_SDMMC_ICR_OFFSET);
    }

  stm32_configwaitints(priv, waitmask, eventset, 0);

  /* Check if the timeout event is specified in the event set */

  if ((priv->waitevents & SDIOWAIT_TIMEOUT) != 0)
    {
      int delay;
      int ret;

      /* Yes.. Handle a cornercase: The user request a timeout event but
       * with timeout == 0?
       */

      if (!timeout)
        {
          scr_seal(priv, -ETIMEDOUT);
          priv->wkupevent = SDIOWAIT_TIMEOUT;
do { leave_critical_section(scrflags); return ; } while (0);
        }

      /* Start the watchdog timer */

      delay = MSEC2TICK(timeout);
      ret   = wd_start(&priv->waitwdog, delay,
                       stm32_eventtimeout, (wdparm_t)priv);
      if (ret < OK)
        {
          scr_seal(priv, ret);
          stm32_endwait(priv, SDIOWAIT_ERROR);
          mcerr("ERROR: wd_start failed: %d\n", ret);
        }
    }

  leave_critical_section(scrflags);
}
static sdio_eventset_t stm32_eventwait(struct sdio_dev_s *dev)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  sdio_eventset_t wkupevent = 0;
  irqstate_t flags;
  int ret;

  /* There is a race condition here... the event may have completed before
   * we get here.  In this case waitevents will be zero, but wkupevents will
   * be non-zero (and, hopefully, the semaphore count will also be non-zero.
   */

  flags = enter_critical_section();

#if defined(CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE)
  /* A call to stm32_waitenable that finds the card ready or
   * a card ejected while in SDIOWAIT_WRCOMPLETE can lead to a
   * condition where there is no waitevents set and no wkupevent
   * This simply means we should not wait.
   */

  if (priv->waitevents == 0 && priv->wkupevent == 0)
    {
      wkupevent = 0;
      goto errout_with_waitints;
    }

#else
  DEBUGASSERT(priv->waitevents != 0 || priv->wkupevent != 0);
#endif

#if defined(CONFIG_MMCSD_SDIOWAIT_WRCOMPLETE)
  if ((priv->waitevents & SDIOWAIT_WRCOMPLETE) != 0)
    {
      /* Atomically read pin to see if ready (true) and determine if ISR
       * fired.  If Pin is ready and if ISR did NOT fire end the wait here.
       */

      if (stm32_gpioread(priv->d0_gpio) &&
          (priv->wkupevent & SDIOWAIT_WRCOMPLETE) == 0)
        {
          stm32_endwait(priv, SDIOWAIT_WRCOMPLETE);
        }
    }
#endif

  /* Loop until the event (or the timeout occurs). Race conditions are
   * avoided by calling stm32_waitenable prior to triggering the logic that
   * will cause the wait to terminate.  Under certain race conditions, the
   * waited-for may have already occurred before this function was called!
   */

  for (; ; )
    {
      /* Wait for an event in event set to occur.  If this the event has
       * already occurred, then the semaphore will already have been
       * incremented and there will be no wait.
       */

      ret = nxsem_wait_uninterruptible(&priv->waitsem);
      if (ret < 0)
        {
          /* Task canceled.  Cancel the wdog (assuming it was started) and
           * return an SDIO error.
           */

          wd_cancel(&priv->waitwdog);
          wkupevent = SDIOWAIT_ERROR;
          goto errout_with_waitints;
        }

      wkupevent = priv->wkupevent;

      /* Check if the event has occurred.  When the event has occurred, then
       * evenset will be set to 0 and wkupevent will be set to a nonzero
       * value.
       */

      if (wkupevent != 0)
        {
          /* Yes... break out of the loop with wkupevent non-zero */

          break;
        }
    }

  /* Disable event-related interrupts */

errout_with_waitints:
  stm32_configwaitints(priv, 0, 0, 0);

  leave_critical_section(flags);
  stm32_dumpsamples(priv);
  return wkupevent;
}
static int stm32_cancel(struct sdio_dev_s *dev)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  irqstate_t flags;

  flags = enter_critical_section();
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
#ifdef CONFIG_STM32H7_SDMMC_IDMA
static int stm32_dmarecvsetup(struct sdio_dev_s *dev,
                              uint8_t *buffer, size_t buflen)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  irqstate_t scrflags = enter_critical_section();
  if (scr_denied(priv) || (priv->scrlease.state == SCR_LEASE_ACTIVE && (priv->scr_completed || buffer != priv->scrlease.arena || buflen != 8)))
    {
      leave_critical_section(scrflags);
      return -EBUSY;
    }


  DEBUGASSERT(priv != NULL && buffer != NULL && buflen > 0);
#if defined(CONFIG_ARCH_HAVE_SDIO_PREFLIGHT)
  DEBUGASSERT(stm32_dmapreflight(dev, buffer, buflen) == 0);
#endif

#if defined(CONFIG_ARMV7M_DCACHE)
  if (((uintptr_t)buffer & (ARMV7M_DCACHE_LINESIZE - 1)) != 0 ||
       (buflen & (ARMV7M_DCACHE_LINESIZE - 1)) != 0)
    {
      /* The read buffer is not cache-line aligned. Read to an internal
       * buffer instead.
       */

      up_invalidate_dcache((uintptr_t)priv->sdmmc_rxbuffer,
                           (uintptr_t)priv->sdmmc_rxbuffer +
                           priv->blocksize);

      priv->unaligned_rx = true;
    }
  else
    {
      up_invalidate_dcache((uintptr_t)buffer,
                           (uintptr_t)buffer + buflen);

      priv->unaligned_rx = false;
    }
#else

  /* IDMA access must be 32 bit aligned */

  priv->unaligned_rx = ((uintptr_t)buffer & 0x3) != 0;

#endif

  /* Reset the DPSM configuration */

  stm32_datadisable(priv);

  /* Initialize register sampling */

  stm32_sampleinit();
  stm32_sample(priv, SAMPLENDX_BEFORE_SETUP);

  /* Save the destination buffer information for use by the interrupt
   * handler.
   */

  priv->buffer      = (uint32_t *)buffer;
  priv->remaining  = buflen;
  priv->receivecnt = buflen;

  /* Then set up the SDIO data path */

  stm32_dataconfig(priv, SDMMC_DTIMER_DATATIMEOUT_MS, buflen, true);

  /* Configure the RX DMA */

  if (priv->unaligned_rx)
    {
      sdmmc_putreg32(priv, (uintptr_t)priv->sdmmc_rxbuffer,
                     STM32_SDMMC_IDMABASE0R_OFFSET);
    }
  else
    {
      sdmmc_putreg32(priv, (uintptr_t)priv->buffer,
                     STM32_SDMMC_IDMABASE0R_OFFSET);
    }

  sdmmc_putreg32(priv, STM32_SDMMC_IDMACTRLR_IDMAEN,
                 STM32_SDMMC_IDMACTRLR_OFFSET);

  /* And enable interrupts */

  stm32_configxfrints(priv, STM32_SDMMC_DMARECV_MASK);

  stm32_sample(priv, SAMPLENDX_AFTER_SETUP);
do { leave_critical_section(scrflags); return OK; } while (0);
}
static void stm32_recvdma(struct stm32_dev_s *priv)
{
  uint32_t dctrl;

  if (priv->unaligned_rx)
    {
      /* If we are receiving multiple blocks to an unaligned buffers,
       * we receive them one-by-one
       */

      /* Copy the received data to client buffer */

      memcpy(priv->buffer, priv->sdmmc_rxbuffer, priv->blocksize);

      /* Invalidate the cache before receiving next block */

      up_invalidate_dcache((uintptr_t)priv->sdmmc_rxbuffer,
                           (uintptr_t)priv->sdmmc_rxbuffer +
                           priv->blocksize);

      /* Update how much there is left to receive */

      priv->remaining -= priv->blocksize;
    }
  else
    {
      /* In an aligned case, we have always received all blocks */

      priv->remaining = 0;
    }

  if (priv->remaining == 0)
    {
      /* no data remaining, end the transfer */

      stm32_endtransfer(priv, SDIOWAIT_TRANSFERDONE);
    }
  else
    {
      /* We end up here only in unaligned rx-buffers case, and are receiving
       * the data one block at a time
       */

      /* Update where to receive the following block */

      priv->buffer = (uint32_t *)((uintptr_t)priv->buffer + priv->blocksize);

      /* Clear pending interrupt status */

      sdmmc_putreg32(priv, STM32_SDMMC_XFRDONE_ICR, STM32_SDMMC_ICR_OFFSET);

      /* Re-enable datapath and wait for next block */

      dctrl = sdmmc_getreg32(priv, STM32_SDMMC_DCTRL_OFFSET);
      dctrl |= STM32_SDMMC_DCTRL_DTEN;
      sdmmc_putreg32(priv, dctrl, STM32_SDMMC_DCTRL_OFFSET);
    }
}
#else
static int stm32_recvsetup(struct sdio_dev_s *dev, uint8_t *buffer,
                           size_t nbytes)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  irqstate_t scrflags = enter_critical_section();
  if (scr_denied(priv) || (priv->scrlease.state == SCR_LEASE_ACTIVE && (priv->scr_completed || buffer != priv->scrlease.arena || nbytes != 8)))
    {
      leave_critical_section(scrflags);
      return -EBUSY;
    }

  irqstate_t flags;

  DEBUGASSERT(priv != NULL && buffer != NULL && nbytes > 0);
  DEBUGASSERT(((uint32_t)buffer & 3) == 0);

  flags = enter_critical_section();
  wd_cancel(&priv->fifowdog);

  /* Reset the DPSM configuration */

  stm32_datadisable(priv);

  /* Initialize register sampling */

  stm32_sampleinit();
  stm32_sample(priv, SAMPLENDX_BEFORE_SETUP);

  /* Save the destination buffer information for use by the interrupt
   * handler
   */

  priv->buffer     = (uint32_t *)buffer;
  priv->remaining  = nbytes;
  priv->receivecnt = nbytes;

  /* Then set up the SDIO data path */

  stm32_dataconfig(priv, SDMMC_DTIMER_DATATIMEOUT_MS, nbytes, true);

  /* Workaround the FIFO data available issue */

  if (nbytes < FIFO_SIZE_IN_BYTES / 2)
    {
      /* Use the driver's buffer */

      sdmmc_putreg32(priv, (uintptr_t)(priv->scrlease.state == SCR_LEASE_ACTIVE ?
                                     priv->scrlease.arena : priv->rxfifo),
                     STM32_SDMMC_IDMABASE0R_OFFSET);

      sdmmc_putreg32(priv, STM32_SDMMC_IDMACTRLR_IDMAEN,
                     STM32_SDMMC_IDMACTRLR_OFFSET);

      /* Ensure CPU will read DMA-ed data */

      up_invalidate_dcache((uintptr_t)priv->rxfifo,
                           (uintptr_t)priv->rxfifo + nbytes);
    }

  /* And enable interrupts */

  stm32_configxfrints(priv, STM32_SDMMC_RECV_MASK);
  stm32_sample(priv, SAMPLENDX_AFTER_SETUP);

  leave_critical_section(flags);
do { leave_critical_section(scrflags); return OK; } while (0);
}
#endif
static int stm32_sdmmc_interrupt(int irq, void *context, void *arg)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)arg;
#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW
  if (scr_closed(priv)) return OK;
#endif
  uint32_t enabled;
  uint32_t status;
  uint32_t pending;
#ifndef CONFIG_STM32H7_SDMMC_IDMA
  irqstate_t flags;
#endif
#if defined(HAVE_SDMMC_SDIO_MODE)
  uint32_t mask;
#endif

  DEBUGASSERT(priv != NULL);

  /* Loop while there are pending interrupts.  Check the SDIO status
   * register.  Mask out all bits that don't correspond to enabled
   * interrupts.  (This depends on the fact that bits are ordered
   * the same in both the STA and MASK register).  If there are non-zero
   * bits remaining, then we have work to do here.
   */

  while ((enabled = (status = sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET)) &
          sdmmc_getreg32(priv, STM32_SDMMC_MASK_OFFSET)) != 0)
    {
      /* Handle in progress, interrupt driven data transfers ****************/

      pending  = enabled & priv->xfrmask;
      if (pending != 0)
        {
          /* Errors take precedence over FIFO service and DATAEND.  Do not
           * copy invalid receive data or report a successful transfer.
           */

          /* Handle data block send/receive CRC failure */

          if ((pending & STM32_SDMMC_STA_DCRCFAIL) != 0)
            {
              /* Terminate the transfer with an error.
               *  Sets STM32_SDMMC_ICR_DCRCFAILC
               */

              mcerr("ERROR: Data block CRC failure, remaining: %u\n",
                    priv->remaining);

              stm32_endtransfer(priv,
                                SDIOWAIT_TRANSFERDONE | SDIOWAIT_ERROR);
            }

          /* Handle data timeout error */

          else if ((pending & STM32_SDMMC_STA_DTIMEOUT) != 0)
            {
              /* Terminate the transfer with an error
               *  Sets STM32_SDMMC_ICR_DTIMEOUTC
               */

              mcerr("ERROR: Data timeout, remaining: %u\n",
                    priv->remaining);

              stm32_endtransfer(priv, SDIOWAIT_TRANSFERDONE |
                                      SDIOWAIT_TIMEOUT);
            }

          /* Handle RX FIFO overrun error */

          else if ((pending & STM32_SDMMC_STA_RXOVERR) != 0)
            {
              /* Terminate the transfer with an error
               * Sets STM32_SDMMC_ICR_RXOVERRC
               */

              mcerr("ERROR: RX FIFO overrun, remaining: %u\n",
                    priv->remaining);

              stm32_endtransfer(priv, SDIOWAIT_TRANSFERDONE |
                                      SDIOWAIT_ERROR);
            }

          /* Handle TX FIFO underrun error */

          else if ((pending & STM32_SDMMC_STA_TXUNDERR) != 0)
            {
              /* Terminate the transfer with an error
               * Sets STM32_SDMMC_ICR_TXUNDERRC
               */

              mcerr("ERROR: TX FIFO underrun, remaining: %u\n",
                    priv->remaining);

              stm32_endtransfer(priv, SDIOWAIT_TRANSFERDONE |
                                      SDIOWAIT_ERROR);
            }
          else if ((pending & STM32_SDMMC_STA_DATAEND) != 0 &&
                   (status & (STM32_SDMMC_STA_IDMATE |
                              STM32_SDMMC_STA_DABORT)) != 0)
            {
#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW
#else
              /* These status flags need not be enabled in MASK.  DATAEND
               * does not authorize copying a failed or aborted receive.
               */

#endif
              stm32_endtransfer(priv,
                                SDIOWAIT_TRANSFERDONE | SDIOWAIT_ERROR);
            }
          else
            {
#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW
              if (priv->scrlease.state == SCR_LEASE_ACTIVE &&
                  (pending & STM32_SDMMC_STA_DATAEND) != 0)
                {
                  uint32_t errors;
                  /* Evidence only: no copy, invalidate, clear, or release. */
                  priv->scr_terminal_sta_before =
                    sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET);
                  priv->scr_terminal_idma =
                    sdmmc_getreg32(priv, STM32_SDMMC_IDMACTRLR_OFFSET);
                  priv->scr_terminal_sta_after =
                    sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET);
                  errors = (priv->scr_terminal_sta_before |
                            priv->scr_terminal_sta_after) &
                    (STM32_SDMMC_STA_IDMATE | STM32_SDMMC_STA_DABORT |
                     STM32_SDMMC_STA_DCRCFAIL | STM32_SDMMC_STA_DTIMEOUT |
                     STM32_SDMMC_STA_RXOVERR | STM32_SDMMC_STA_TXUNDERR);
                  priv->scr_completed = errors == 0;
                  stm32_endtransfer(priv, SDIOWAIT_TRANSFERDONE |
                    (errors ? SDIOWAIT_ERROR : 0));
                  return OK;
                }
#endif
#ifndef CONFIG_STM32H7_SDMMC_IDMA
              /* Is the RX FIFO half full or more?  Is so then we must be
               * processing a receive transaction.
               */

              if ((pending & STM32_SDMMC_STA_RXFIFOHF) != 0)
                {
                  /* Receive data from the RX FIFO */

                  stm32_recvfifo(priv);
                  flags = enter_critical_section();
                  if (priv->receivecnt && priv->remaining &&
                      !WDOG_ISACTIVE(&priv->fifowdog))
                    {
                      wd_start(&priv->fifowdog, 1, stm32_sdmmc_fifo_monitor,
                               (wdparm_t)priv);
                    }

                  leave_critical_section(flags);
                }

              /* Otherwise, Is the transmit FIFO half empty or less?  If so
               * we must be processing a send transaction.  NOTE:  We can't
               * be processing both!
               */

              else if ((pending & STM32_SDMMC_STA_TXFIFOHE) != 0)
                {
                  /* Send data via the TX FIFO */

                  stm32_sendfifo(priv);

                  /* If we are done shutdown the TXFIFOHE interrupt */

                  if (priv->remaining == 0)
                    {
                      sdmmc_putreg32(priv, ~STM32_SDMMC_MASK_TXFIFOHEIE &
                                     sdmmc_getreg32(priv,
                                                    STM32_SDMMC_MASK_OFFSET),
                                     STM32_SDMMC_MASK_OFFSET);
                    }
                }
#endif

              /* Handle data end events */

              if ((pending & STM32_SDMMC_STA_DATAEND) != 0)
                {
                  /* Handle any data remaining the RX FIFO.  If the RX FIFO is
                   * less than half full at the end of the transfer, then no
                   * half-full interrupt will be received.
                   */

#ifndef CONFIG_STM32H7_SDMMC_IDMA

                  flags = enter_critical_section();

                  /* If the transfer would not trigger fifo half full
                   * we used IDMA to manage the lame fifo
                   */

                  if (priv->remaining && priv->remaining <
                      FIFO_SIZE_IN_BYTES / 2)
                    {
                      memcpy(priv->buffer, priv->rxfifo, priv->remaining);
                    }

                  stm32_endtransfer(priv, SDIOWAIT_TRANSFERDONE);
                  leave_critical_section(flags);
#else
                  if (priv->receivecnt)
                    {
                      /* Invalidate dcache, and copy the received data into
                       * client buffers in unaligned case
                       */

                      stm32_recvdma(priv);
                    }
                  else
                    {
                      /* Then terminate the transfer.
                       * Sets STM32_SDMMC_ICR_DATAENDC
                       */

                      stm32_endtransfer(priv, SDIOWAIT_TRANSFERDONE);
                    }
#endif
                }
            }
        }

      /* Handle wait events *************************************************/

      pending  = enabled & priv->waitmask;
      if (pending != 0)
        {
          /* Is this a response completion event? */

          if ((pending & STM32_SDMMC_RESPDONE_STA) != 0)
            {
              sdmmc_putreg32(priv, STM32_SDMMC_RESPDONE_STA,
                             STM32_SDMMC_ICR_OFFSET);

              /* Yes.. Is there a thread waiting for response done? */

              if ((priv->waitevents & SDIOWAIT_RESPONSEDONE) != 0)
                {
                  /* Yes.. wake the thread up */

                   stm32_endwait(priv, SDIOWAIT_RESPONSEDONE);
                }
            }

          /* Is this a command completion event? */

          if ((pending & STM32_SDMMC_CMDDONE_STA) != 0)
            {
              sdmmc_putreg32(priv, STM32_SDMMC_CMDDONE_STA,
                             STM32_SDMMC_ICR_OFFSET);

              /* Yes.. Is there a thread waiting for command done? */

              if ((priv->waitevents & SDIOWAIT_RESPONSEDONE) != 0)
                {
                  /* Yes.. wake the thread up */

                  stm32_endwait(priv, SDIOWAIT_CMDDONE);
                }
            }
        }

#if defined(HAVE_SDMMC_SDIO_MODE)
      if (priv->sdiomode == true)
        {
          pending = enabled & priv->sdiointmask;
          if (pending != 0)
            {
              mask = sdmmc_getreg32(priv, STM32_SDMMC_MASK_OFFSET);

              /* Clear the mask so we don't get call'd again */

              sdmmc_putreg32(priv, mask & ~STM32_SDMMC_MASK_SDIOITIE,
                             STM32_SDMMC_MASK_OFFSET);

              /* Now clear the interruption */

              sdmmc_putreg32(priv, STM32_SDMMC_ICR_SDIOITC,
                             STM32_SDMMC_ICR_OFFSET);

              /* Call the ISR that has been registered */

              if (priv->do_sdio_card)
                {
                  priv->do_sdio_card(priv->do_sdio_arg);
                }
            }
        }
#endif
    }

  return OK;
}
static int irqfirst,carderror,transporterror;static unsigned calls16,calls55,calls51;
struct mmcsd_state_s {struct sdio_dev_s*dev;uint32_t caps,selblocklen,rca;bool locked;};
static int send_hook(struct sdio_dev_s*d,uint32_t cmd,uint32_t arg){int r=stm32_sendcmd(d,cmd,arg);if(r)return r;unsigned index=cmd&MMCSD_CMDIDX_MASK;regs[9]=index;regs[10]=0;regs[0]=(1u<<7);if(index==16)calls16++;if(index==55)calls55++;if(index==51){calls51++;if(transporterror){regs[0]=STM32_SDMMC_STA_CCRCFAIL;return 0;}if(carderror)regs[10]=MMCSD_R1_ERRORMASK;
struct stm32_dev_s*p=(void*)d;assert(regs[8]==(uintptr_t)p->scrlease.arena);assert(regs[7]==8);assert((regs[4]&0xf2)==0x32);memset((void*)regs[8],0x5a,8);regs[6]=0;
if(irqfirst){regs[0]|=STM32_SDMMC_STA_DATAEND;assert(stm32_sdmmc_interrupt(0,NULL,p)==0);}}
return 0;}
static int r1_hook(struct sdio_dev_s*d,uint32_t cmd,uint32_t*r){int ret=stm32_recvshortcrc(d,cmd,r);if(!ret&&(cmd&MMCSD_CMDIDX_MASK)==51&&!irqfirst){regs[0]|=STM32_SDMMC_STA_DATAEND;assert(stm32_sdmmc_interrupt(0,NULL,d)==0);}return ret;}
#define SDIO_SENDCMD send_hook
#define SDIO_WAITRESPONSE stm32_waitresponse
#define SDIO_RECVR1 r1_hook
#define SDIO_BLOCKSETUP stm32_blocksetup
#ifdef CONFIG_STM32H7_SDMMC_IDMA
#define SDIO_DMARECVSETUP stm32_dmarecvsetup
#define SDIO_RECVSETUP stm32_dmarecvsetup
#else
#define SDIO_RECVSETUP stm32_recvsetup
#endif
#define SDIO_WAITENABLE stm32_waitenable
#define SDIO_EVENTWAIT stm32_eventwait
#define SDIO_CANCEL stm32_cancel
static int mmcsd_sendcmdpoll(FAR struct mmcsd_state_s *priv, uint32_t cmd,
                             uint32_t arg)
{
  int ret;

  /* Send the command */

  ret = SDIO_SENDCMD(priv->dev, cmd, arg);
  if (ret == OK)
    {
      /* Then poll-wait until the response is available */

      ret = SDIO_WAITRESPONSE(priv->dev, cmd);
      if (ret != OK)
        {
          ferr("ERROR: Wait for response to cmd: %08" PRIx32
               " failed: %d\n",
               cmd, ret);
        }
    }

  return ret;
}
static int mmcsd_recv_r1(FAR struct mmcsd_state_s *priv, uint32_t cmd)
{
  uint32_t r1;
  int ret;

  /* Get the R1 response from the hardware */

  ret = SDIO_RECVR1(priv->dev, cmd, &r1);
  if (ret == OK)
    {
      /* Check if R1 reports an error */

      if ((r1 & MMCSD_R1_ERRORMASK) != 0)
        {
          /* Card locked is considered an error. Save the card locked
           * indication for later use.
           */

          ferr("ERROR: R1=%08" PRIx32 "\n", r1);
          priv->locked = ((r1 & MMCSD_R1_CARDISLOCKED) != 0);
          ret = -EIO;
        }
    }

  return ret;
}
static int mmcsd_setblocklen(FAR struct mmcsd_state_s *priv,
                             uint32_t blocklen)
{
  int ret = OK;

  /* Is the block length already selected in the card? */

  if (priv->selblocklen != blocklen)
    {
      /* Send CMD16 = SET_BLOCKLEN.  This command sets the block length (in
       * bytes) for all following block commands (read and write). Default
       * block length is specified in the CSD.
       */

      mmcsd_sendcmdpoll(priv, MMCSD_CMD16, blocklen);
      ret = mmcsd_recv_r1(priv, MMCSD_CMD16);
      if (ret == OK)
        {
          priv->selblocklen = blocklen;
        }
      else
        {
          ferr("ERROR: mmcsd_recv_r1 for CMD16 failed: %d\n", ret);
        }
    }

  return ret;
}
static int mmcsd_eventwait(FAR struct mmcsd_state_s *priv,
                           sdio_eventset_t failevents)
{
  sdio_eventset_t wkupevent;

  /* Wait for the set of events enabled by SDIO_EVENTENABLE. */

  wkupevent = SDIO_EVENTWAIT(priv->dev);

  /* SDIO_EVENTWAIT returns the event set containing the event(s) that ended
   * the wait.  It should always be non-zero, but may contain failure as
   * well as success events.  Check if it contains any failure events.
   */

  if ((wkupevent & failevents) != 0)
    {
      /* Yes.. the failure event is probably SDIOWAIT_TIMEOUT */

      ferr("ERROR: Awakened with %02x\n", wkupevent);
      return wkupevent & SDIOWAIT_TIMEOUT ? -ETIMEDOUT : -EIO;
    }

  /* Since there are no failure events, we must have been awakened by one
   * (or more) success events.
   */

  return OK;
}
static int mmcsd_get_scr_data(FAR struct mmcsd_state_s *priv, uint32_t scr[2])
{
  int ret;
#ifdef CONFIG_SDIO_DMA
  bool drop = (priv->caps & SDIO_CAPS_DMASUPPORTED) == 0;
#endif

#if defined(CONFIG_SDIO_DMA) && defined(CONFIG_ARCH_HAVE_SDIO_PREFLIGHT)
  /* If we think we are going to perform a DMA transfer, make sure that we
   * will be able to before we commit the card to the operation.
   */

  if ((priv->caps & SDIO_CAPS_DMASUPPORTED) != 0)
    {
      ret = SDIO_DMAPREFLIGHT(priv->dev, (FAR uint8_t *)scr, 8);
      if (ret != OK)
        {
          finfo("SDIO_DMAPREFLIGHT: error %d, drop to RECVSETUP\n", ret);
          drop = true;
        }
    }
#endif

  /* Set Block Size To 8 Bytes */

  ret = mmcsd_setblocklen(priv, 8);
  if (ret != OK)
    {
      ferr("ERROR: mmcsd_setblocklen failed: %d\n", ret);
      return ret;
    }

  /* Setup up to receive data with interrupt mode */

  SDIO_BLOCKSETUP(priv->dev, 8, 1);
#ifdef CONFIG_SDIO_DMA
  if (!drop && (priv->caps & SDIO_CAPS_DMASUPPORTED) != 0)
    {
      ret = SDIO_DMARECVSETUP(priv->dev, (FAR uint8_t *)scr, 8);
      if (ret != OK)
        {
          finfo("SDIO_DMARECVSETUP: error %d, drop to RECVSETUP\n", ret);
          drop = true;
        }
    }

  if (drop)
#endif
    {
      ret = SDIO_RECVSETUP(priv->dev, (FAR uint8_t *)scr, 8);
      if (ret != OK)
        {
          ferr("ERROR: SDIO_RECVSETUP for SCR failed: %d\n", ret);
          return ret;
        }
    }

  SDIO_WAITENABLE(priv->dev,
                  SDIOWAIT_TRANSFERDONE | SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR,
                  MMCSD_SCR_DATADELAY);

  /* Send CMD55 APP_CMD with argument as card's RCA */

#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW
  ret = mmcsd_sendcmdpoll(priv, SD_CMD55, (uint32_t)priv->rca << 16);
  if (ret != OK)
    {
      ferr("ERROR: Send/poll SD_CMD55 failed: %d\n", ret);
      SDIO_CANCEL(priv->dev);
      return ret;
    }

#else
  mmcsd_sendcmdpoll(priv, SD_CMD55, (uint32_t)priv->rca << 16);
#endif

  ret = mmcsd_recv_r1(priv, SD_CMD55);
  if (ret != OK)
    {
      ferr("ERROR: RECVR1 for CMD55 failed: %d\n", ret);
      SDIO_CANCEL(priv->dev);
      return ret;
    }

  /* Send ACMD51 SD_APP_SEND_SCR with argument as 0 to start data receipt */

#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW
  ret = mmcsd_sendcmdpoll(priv, SD_ACMD51, 0);
  if (ret != OK)
    {
      ferr("ERROR: Send/poll SD_ACMD51 failed: %d\n", ret);
      SDIO_CANCEL(priv->dev);
      return ret;
    }

#else
  mmcsd_sendcmdpoll(priv, SD_ACMD51, 0);
#endif

  ret = mmcsd_recv_r1(priv, SD_ACMD51);
  if (ret != OK)
    {
      ferr("ERROR: RECVR1 for ACMD51 failed: %d\n", ret);
      SDIO_CANCEL(priv->dev);
      return ret;
    }

  /* Wait for data to be transferred */

  ret = mmcsd_eventwait(priv, SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR);
  if (ret != OK)
    {
      ferr("ERROR: mmcsd_eventwait for READ DATA failed: %d\n", ret);
    }

  return ret;
}
static int mmcsd_get_scr(FAR struct mmcsd_state_s *priv, uint32_t scr[2])
{
  struct scr_lease_key key;
  uint8_t *arena;
  int ret;
  /* Caller pointer is used only by task-side qualified final copy. */
  ret = stm32_scr_claim(priv->dev, &arena, &key);
  if (ret != OK) return ret;
  ret = mmcsd_get_scr_data(priv, (uint32_t *)arena);
  ret = stm32_scr_finish(priv->dev, key, ret, scr);
  ferr("SCR review: result=%d, stop_status=%d (diagnostic only)\n",
       ret, stm32_scr_stop_status(priv->dev, key));
  return ret;
}

int main(void){void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);struct stm32_dev_s*p=m;observed=p;
unsigned bad[]={STM32_SDMMC_STA_DPSMACT,STM32_SDMMC_STA_CPSMACT,STM32_SDMMC_STA_DTIMEOUT,STM32_SDMMC_STA_DCRCFAIL,STM32_SDMMC_STA_IDMATE,STM32_SDMMC_STA_DABORT,0};
for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++){memset(p,0,sizeof *p);memset(regs,0,sizeof regs);chipid=0x20030450;ccr=0;uint8_t*a;struct scr_lease_key k;assert(stm32_scr_claim(&p->dev,&a,&k)==0);p->scr_phase=4;p->scr_completed=true;regs[0]=bad[i]?(bad[i]|STM32_SDMMC_STA_DATAEND):0;count_reads=0;uint32_t out[2]={123,456};assert(stm32_scr_finish(&p->dev,k,0,out)==-EAGAIN);assert(count_reads==0&&out[0]==123&&out[1]==456&&p->scrlease.state==SCR_LEASE_QUARANTINED);}
memset(p,0,sizeof *p);memset(regs,0,sizeof regs);chipid=0x20030450;uint8_t*a;struct scr_lease_key k;assert(stm32_scr_claim(&p->dev,&a,&k)==0);p->scr_phase=4;p->xfrmask=STM32_SDMMC_STA_DATAEND;regs[0]=STM32_SDMMC_STA_DATAEND|STM32_SDMMC_STA_DPSMACT;count_reads=0;assert(stm32_sdmmc_interrupt(0,NULL,p)==0);assert(count_reads==0);uint32_t out[2]={123,456};assert(stm32_scr_finish(&p->dev,k,0,out)==-EAGAIN);assert(count_reads==0&&out[0]==123&&out[1]==456);
munmap(m,4096);puts("PASS current finish short-circuits before DCOUNT read for active CPSM/DPSM, error/DABORT or missing DATAEND. Host read-order assertion, not physical quiescence proof.");}
