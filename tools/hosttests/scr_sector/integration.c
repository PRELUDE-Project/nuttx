/* SPDX-License-Identifier: Apache-2.0 */
/* Extracted Apache NuttX functions with host-only test doubles. */
#define CONFIG_SD_SECTOR_READ_REVIEW 1
#define CONFIG_SD_RECEIVE_LEASE_ONLY_REVIEW 1
#define CONFIG_STM32H7_SDMMC_IDMA 1
#define CONFIG_STM32H7_SDMMC1 1
#define CONFIG_STM32H7_SDMMC2 1
#define _GNU_SOURCE
#include <sys/mman.h>

#include "scr_lease_review.h"
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
#define STM32_SDMMC_DCTRL_512BYTES (9u<<4)
#define SECTOR_R1_ERRORMASK 0xfdffe088u
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
struct stm32_dev_s{struct sdio_dev_s dev;unsigned scr_phase;bool scr_completed,sector_mode,sector_attempted,scr_success_seen,scr_rx_setup;uint32_t sector_argument;uint32_t scr_terminal_sta_before,scr_terminal_dcount,scr_terminal_idma,scr_terminal_sta_after;bool scrinitializing,onebit;uintptr_t scrinitowner;struct scr_lease_review scrlease;struct scr_lease_key scrkey;unsigned xfrmask,waitmask,waitevents;uint8_t cdstatus;size_t remaining;unsigned receivecnt,blocksize;uint32_t *buffer;uint8_t rxfifo[64],sdmmc_rxbuffer[64];bool fifowdog,waitwdog,unaligned_rx;unsigned wkupevent;int waitsem;};
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

#include "actual-command-constants.h"

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

typedef uint8_t sdio_statset_t;
#define SDIO_STATUS_PRESENT 1
#define SDIO_STATUS_WRPROTECTED 2
static void stm32_callback(void*p){(void)p; /* actual opt-in callback returns immediately */}

static struct stm32_dev_s g_sdmmcdev1,g_sdmmcdev2;
static void stm32_recvdma(struct stm32_dev_s*p){(void)p;assert(!"generic DMA completion must not run for sector lease");}
static void stm32_endwait(struct stm32_dev_s*,sdio_eventset_t);
static void stm32_endtransfer(struct stm32_dev_s*,sdio_eventset_t);
static bool scr_closed(struct stm32_dev_s *priv)
{
  return
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
         priv->scrlease.state == SCR_LEASE_HANDOFF ||
#endif
         priv->scrlease.state == SCR_LEASE_QUARANTINED ||
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
         address - STM32_AXISRAM_BASE <= STM32H7_SRAM_SIZE - sizeof(((struct scr_lease_review *)0)->arena);
}
static size_t scr_receive_size(struct stm32_dev_s *priv)
{
  return priv->sector_mode ? 512 : 8;
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
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
      priv->sector_mode = false;
#endif
#ifdef CONFIG_SD_RECEIVE_LEASE_ONLY_REVIEW
      priv->scr_rx_setup = false;
#endif
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
int stm32_sector_claim(struct sdio_dev_s *dev, uint64_t lba, uint64_t capacity,
                        bool block_addressed_sd, uint8_t **arena,
                        struct scr_lease_key *key)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  irqstate_t f = enter_critical_section();
  int ret = -EBUSY;
  if (!block_addressed_sd || capacity == 0 ||
      capacity > (uint64_t)UINT32_MAX + 1 || lba >= capacity ||
      lba > UINT32_MAX || arena == NULL || key == NULL)
    { leave_critical_section(f); return -EINVAL; }
  if (up_interrupt_context() || !priv->scr_success_seen ||
      priv->sector_attempted || priv->scrlease.state != SCR_LEASE_HANDOFF)
    { leave_critical_section(f); return -EBUSY; }
  /* One attempt, even when the clean-handoff checks reject it. */
  priv->sector_attempted = true;
  priv->scrlease.state = SCR_LEASE_IDLE;
  ret = stm32_scr_claim(dev, arena, key);
  if (ret == OK)
    {
      priv->sector_mode = true;
      priv->sector_argument = (uint32_t)lba;
    }
  if (ret != OK && priv->scrlease.state == SCR_LEASE_IDLE)
    {
      priv->scrlease.state = SCR_LEASE_RETIRED;
      priv->scrlease.result = ret;
      priv->scrlease.cpu_destination = NULL;
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
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
  if (priv->sector_mode)
    { leave_critical_section(flags); return -EINVAL; }
#endif
  if (!scr_lease_matches(&priv->scrlease, key) || up_interrupt_context() ||
      key.owner != (uintptr_t)nxsched_gettid() ||
      priv->scrlease.state == SCR_LEASE_IDLE)
    goto out;
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
  if (priv->scrlease.state == SCR_LEASE_HANDOFF)
    { ret = -EALREADY; goto out; }
#endif
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
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
  priv->scr_success_seen = true;
#endif
  priv->scrlease.cpu_destination = NULL;
#ifndef CONFIG_SD_SECTOR_READ_REVIEW
  priv->scrlease.owner = 0;
#endif
  priv->scrlease.result = 0;
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
  priv->scrlease.state = SCR_LEASE_HANDOFF;
#else
  priv->scrlease.state = SCR_LEASE_IDLE;
#endif
  priv->scr_completed = false;
  priv->scr_phase = 0;
  ret = 0;
out:
  leave_critical_section(flags);
  return ret;
}
static bool sector_output_overlaps(uintptr_t out)
{
#ifdef CONFIG_STM32H7_SDMMC1
  if (out < (uintptr_t)&g_sdmmcdev1 + sizeof(g_sdmmcdev1) &&
      out + 512u > (uintptr_t)&g_sdmmcdev1) return true;
#endif
#ifdef CONFIG_STM32H7_SDMMC2
  if (out < (uintptr_t)&g_sdmmcdev2 + sizeof(g_sdmmcdev2) &&
      out + 512u > (uintptr_t)&g_sdmmcdev2) return true;
#endif
  return false;
}
int stm32_sector_finish(struct sdio_dev_s *dev, struct scr_lease_key key,
                     int result, uint8_t output[512])
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  irqstate_t flags = enter_critical_section();
  uint32_t sta;
  uint32_t idma;
  int ret = -ESTALE;
  uintptr_t dst = (uintptr_t)output;
  if (!priv->sector_mode)
    { leave_critical_section(flags); return -EINVAL; }
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

      if (priv->scrlease.state == SCR_LEASE_QUARANTINED &&
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
      ret = priv->scrlease.state == SCR_LEASE_RETIRED ? -EALREADY : priv->scrlease.result;
      goto out;
    }
  if (!output || dst > UINTPTR_MAX - 512u || sector_output_overlaps(dst) ||
      (dst < (uintptr_t)priv + sizeof(*priv) && dst + 512u > (uintptr_t)priv))
    { scr_seal(priv, -EINVAL); ret = -EINVAL; goto out; }
  sta = sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET);
  idma = sdmmc_getreg32(priv, STM32_SDMMC_IDMACTRLR_OFFSET);
  if (!output || !priv->scr_completed || priv->scr_phase != 2 ||
      !scr_platform_qualified() ||
      !scr_arena_qualified((uintptr_t)priv->scrlease.arena) ||
      (sta & (SCR_HW_ERRORS | SCR_HW_ACTIVE)) != 0 ||
      (sta & STM32_SDMMC_STA_DATAEND) == 0 ||
      sdmmc_getreg32(priv, STM32_SDMMC_DCOUNT_OFFSET) != 0 ||
      sdmmc_getreg32(priv, STM32_SDMMC_DLEN_OFFSET) != 512 ||
      sdmmc_getreg32(priv, STM32_SDMMC_IDMABASE0R_OFFSET) !=
        (uintptr_t)priv->scrlease.arena ||
      (idma & STM32_SDMMC_IDMACTRLR_IDMABMODE) != 0 ||
      (sdmmc_getreg32(priv, STM32_SDMMC_DCTRL_OFFSET) &
       (STM32_SDMMC_DCTRL_DTDIR | STM32_SDMMC_DCTRL_DTMODE_MASK |
        STM32_SDMMC_DCTRL_DBLOCKSIZE_MASK)) !=
       (STM32_SDMMC_DCTRL_DTDIR | STM32_SDMMC_DCTRL_DTMODE_BLOCK |
        STM32_SDMMC_DCTRL_512BYTES) || priv->xfrmask != 0 ||
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
  memcpy(output, priv->scrlease.arena, 512);
  priv->scrlease.cpu_destination = NULL;
  /* Retain owner/key for diagnostics; never reopen controller admission. */
  priv->scrlease.result = 0;
  priv->scrlease.state = SCR_LEASE_RETIRED;
  priv->scr_completed = false;
  priv->scr_phase = 0;
  ret = 0;
out:
  leave_critical_section(flags);
  return ret;
}
static int stm32_sendcmd(struct sdio_dev_s *dev, uint32_t cmd,
                         uint32_t arg)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW
  irqstate_t scrflags = enter_critical_section();
  if (scr_denied(priv))
    {
      leave_critical_section(scrflags);
      return -EBUSY;
    }

  if (priv->scrlease.state == SCR_LEASE_ACTIVE)
    {
      unsigned index = (cmd & MMCSD_CMDIDX_MASK) >> MMCSD_CMDIDX_SHIFT;
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
      if (priv->sector_mode)
        {
          if (priv->scr_completed || priv->scr_phase != 0 ||
              !priv->scr_rx_setup || cmd != MMCSD_CMD17 ||
              arg != priv->sector_argument)
            { leave_critical_section(scrflags); return -EBUSY; }
          priv->scr_phase = 1;
        }
      else
#endif
        {
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
    }

#endif
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
#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW
do { leave_critical_section(scrflags); return OK; } while (0);
#else
  return OK;
#endif
}
static int stm32_recvshortcrc(struct sdio_dev_s *dev, uint32_t cmd,
                              uint32_t *rshort)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW
  irqstate_t scrflags = enter_critical_section();
  if (scr_denied(priv))
    {
      leave_critical_section(scrflags);
      return -EBUSY;
    }

#endif
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
#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW
  if (ret == OK && priv->scrlease.state == SCR_LEASE_ACTIVE)
    {
      unsigned index = (cmd & MMCSD_CMDIDX_MASK) >> MMCSD_CMDIDX_SHIFT;
      unsigned received = sdmmc_getreg32(priv, STM32_SDMMC_RESPCMD_OFFSET) &
                          STM32_SDMMC_RESPCMD_MASK;
      if (index != received) ret = -EIO;
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
      else if (priv->sector_mode)
        {
          if (cmd != MMCSD_CMD17 || priv->scr_phase != 1 ||
              (*rshort & SECTOR_R1_ERRORMASK) != 0) ret = -EIO;
          else priv->scr_phase = 2;
        }
#endif
      else if (index == 55 && priv->scr_phase == 1) priv->scr_phase = 2;
      else if (index == 51 && priv->scr_phase == 3) priv->scr_phase = 4;
      else if (cmd != MMCSD_CMD16 || priv->scr_phase != 0) ret = -EIO;
    }

#ifdef CONFIG_SD_SECTOR_READ_REVIEW
  if (priv->sector_mode && ret < 0) scr_seal(priv, ret);
#endif
do { leave_critical_section(scrflags); return ret; } while (0);
#else
  return ret;
#endif
}
static void stm32_endwait(struct stm32_dev_s *priv,
                          sdio_eventset_t wkupevent)
{
#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW
  if (priv->scrlease.state != SCR_LEASE_IDLE)
    {
      if (wkupevent == SDIOWAIT_TRANSFERDONE && priv->scr_completed)
        scr_close_producers(priv);
      else
        scr_seal(priv, (wkupevent & SDIOWAIT_TIMEOUT) ? -ETIMEDOUT : -EIO);
    }

#endif
  /* Cancel the watchdog timeout */

  wd_cancel(&priv->waitwdog);

  /* Disable event-related interrupts */

  stm32_configwaitints(priv, 0, 0, wkupevent);

  /* Wake up the waiting thread */

  nxsem_post(&priv->waitsem);
}
static void stm32_endtransfer(struct stm32_dev_s *priv,
                              sdio_eventset_t wkupevent)
{
  irqstate_t flags = enter_critical_section();
#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW

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
#endif

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
static int stm32_dmarecvsetup(struct sdio_dev_s *dev,
                              uint8_t *buffer, size_t buflen)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW
  irqstate_t scrflags = enter_critical_section();
#ifdef CONFIG_SD_RECEIVE_LEASE_ONLY_REVIEW
  /* Boot-time diagnostic restriction, not a recovery or DMA-stop method. */
  if (priv->scrlease.state != SCR_LEASE_ACTIVE || priv->scr_rx_setup)
    {
      leave_critical_section(scrflags);
      return -ENOTSUP;
    }
#endif
  if (scr_denied(priv) || (priv->scrlease.state == SCR_LEASE_ACTIVE && (priv->scr_completed || buffer != priv->scrlease.arena || buflen != scr_receive_size(priv))))
    {
      leave_critical_section(scrflags);
      return -EBUSY;
    }

#endif
#ifdef CONFIG_SD_RECEIVE_LEASE_ONLY_REVIEW
  priv->scr_rx_setup = true;
#endif


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
#ifdef CONFIG_SD_SCR_CONTAINMENT_REVIEW
do { leave_critical_section(scrflags); return OK; } while (0);
#else
  return OK;
#endif
}
void sdio_mediachange(struct sdio_dev_s *dev, bool cardinslot)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  sdio_statset_t cdstatus;
  irqstate_t flags;

  /* Update card status */

  flags = enter_critical_section();
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
  if ((priv->scrlease.state != SCR_LEASE_IDLE || !cardinslot || (priv->cdstatus & SDIO_STATUS_PRESENT) != 0))
    {
      if (priv->scrlease.state == SCR_LEASE_IDLE)
        {
          priv->scrlease.state = SCR_LEASE_RETIRED;
          priv->scrlease.result = -ENODEV;
          scr_close_producers(priv);
        }
      else scr_seal(priv, -ENODEV);
      if (priv->waitevents != 0) stm32_endwait(priv, SDIOWAIT_ERROR);
      leave_critical_section(flags);
      return;
    }
#endif

  cdstatus = priv->cdstatus;
  if (cardinslot)
    {
      priv->cdstatus |= SDIO_STATUS_PRESENT;
    }
  else
    {
      priv->cdstatus &= ~SDIO_STATUS_PRESENT;
    }

  leave_critical_section(flags);

  mcinfo("cdstatus OLD: %02" PRIx8 " NEW: %02" PRIx8 "\n",
         cdstatus, priv->cdstatus);

  /* Perform any requested callback if the status has changed */

  if (cdstatus != priv->cdstatus)
    {
      stm32_callback(priv);
    }
}
void sdio_wrprotect(struct sdio_dev_s *dev, bool wrprotect)
{
  struct stm32_dev_s *priv = (struct stm32_dev_s *)dev;
  irqstate_t flags;

  /* Update card status */

  flags = enter_critical_section();
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
  if (true)
    {
      if (priv->scrlease.state == SCR_LEASE_IDLE)
        {
          priv->scrlease.state = SCR_LEASE_RETIRED;
          priv->scrlease.result = -ENODEV;
          scr_close_producers(priv);
        }
      else scr_seal(priv, -ENODEV);
      if (priv->waitevents != 0) stm32_endwait(priv, SDIOWAIT_ERROR);
      leave_critical_section(flags);
      return;
    }
#endif

  if (wrprotect)
    {
      priv->cdstatus |= SDIO_STATUS_WRPROTECTED;
    }
  else
    {
      priv->cdstatus &= ~SDIO_STATUS_WRPROTECTED;
    }

  mcinfo("cdstatus: %02" PRIx8 "\n", priv->cdstatus);
  leave_critical_section(flags);
}
static void fresh(struct stm32_dev_s*p){memset(p,0,sizeof(*p));memset(regs,0,sizeof regs);current_owner=1;depth=0;chipid=0x20030450;ccr=0;fail_disable=false;late_error=0;observed=p;}
static struct scr_lease_key begin(struct stm32_dev_s*p){uint8_t*a;struct scr_lease_key k;p->scr_success_seen=true;p->scrlease.state=SCR_LEASE_HANDOFF;assert(stm32_sector_claim(&p->dev,1,4096,true,&a,&k)==0);assert(a==p->scrlease.arena&&sizeof p->scrlease.arena==512);return k;}
static void setup(struct stm32_dev_s*p){p->blocksize=512;assert(stm32_dmarecvsetup(&p->dev,p->scrlease.arena,512)==0);assert(regs[8]==(uintptr_t)p->scrlease.arena);assert(stm32_dmarecvsetup(&p->dev,p->scrlease.arena,512)==-ENOTSUP);}
static void command(struct stm32_dev_s*p){assert(stm32_sendcmd(&p->dev,MMCSD_CMD17,2)==-EBUSY);assert(stm32_sendcmd(&p->dev,MMCSD_CMD17,1)==0);assert(p->scr_phase==1);regs[9]=17;regs[10]=0;regs[0]=0;uint32_t r;assert(stm32_recvshortcrc(&p->dev,MMCSD_CMD17,&r)==0&&p->scr_phase==2);}
static void terminal(struct stm32_dev_s*p,unsigned errors){p->waitevents=SDIOWAIT_TRANSFERDONE|SDIOWAIT_ERROR|SDIOWAIT_TIMEOUT;regs[0]=STM32_SDMMC_STA_DATAEND|errors;regs[6]=0;regs[7]=512;regs[8]=(uintptr_t)p->scrlease.arena;regs[4]=STM32_SDMMC_DCTRL_DTDIR|STM32_SDMMC_DCTRL_512BYTES;regs[5]=1;assert(stm32_sdmmc_interrupt(0,NULL,p)==0);}

#define MMCSD_CARDTYPE_SDV2 4
#define MMCSD_CARDTYPE_BLOCK 8
#define MMCSD_BLOCK_RDATADELAY 100
struct mmcsd_state_s{struct sdio_dev_s*dev;unsigned type,csd[4],blocksize,blockshift;};
static struct mmcsd_state_s g_scrdiag_state;
static bool g_scrdiag_ready,g_sector_attempted;
static int inject;
/* Block and wait arm are contract stubs; timer scheduling is not executed. */
#define SDIO_BLOCKSETUP(d,b,n) do{assert((b)==512&&(n)==1);((struct stm32_dev_s*)(d))->blocksize=(b);}while(0)
#define SDIO_WAITENABLE(d,e,t) do{assert((t)==100);((struct stm32_dev_s*)(d))->waitevents=(e);}while(0)
#define SDIO_DMARECVSETUP stm32_dmarecvsetup
static int mmcsd_sendcmdpoll(struct mmcsd_state_s*p,unsigned c,uint32_t a){int r=stm32_sendcmd(p->dev,c,a);if(r)return r;/* poll scheduling mocked */return inject==1?-ETIMEDOUT:0;}
static int mmcsd_recv_r1(struct mmcsd_state_s*p,unsigned c){uint32_t r;regs[9]=17;regs[10]=inject==2?SECTOR_R1_ERRORMASK:0;regs[0]=0;return stm32_recvshortcrc(p->dev,c,&r);}
static int mmcsd_eventwait(struct mmcsd_state_s*q,unsigned e){struct stm32_dev_s*p=(void*)q->dev;assert(e==(SDIOWAIT_TIMEOUT|SDIOWAIT_ERROR));memset(p->scrlease.arena,0x69,512);if(inject==3){stm32_endwait(p,SDIOWAIT_TIMEOUT);return -ETIMEDOUT;}terminal(p,inject==4?STM32_SDMMC_STA_IDMATE:0);return p->scrlease.state==SCR_LEASE_QUARANTINED?-EIO:0;}
static int mmcsd_sector_validate(struct mmcsd_state_s *priv, uint64_t lba,
                                 uint64_t *capacity)
{
  if (priv->type != (MMCSD_CARDTYPE_SDV2 | MMCSD_CARDTYPE_BLOCK) ||
      (priv->csd[0] >> 30) != 1 || ((priv->csd[1] >> 16) & 15) != 9 ||
      priv->blocksize != 512 || priv->blockshift != 9)
    return -ENOTSUP;
  uint32_t csize = ((priv->csd[1] & 0x3f) << 16) | (priv->csd[2] >> 16);
  uint64_t blocks = ((uint64_t)csize + 1) << 10;
  if (lba >= blocks || lba > UINT32_MAX) return -ERANGE;
  *capacity = blocks;
  return OK;
}int mmcsd_sector_readonce(struct sdio_dev_s *dev, uint64_t lba,
                           uint8_t *output, size_t output_size)
{
  struct mmcsd_state_s *priv = &g_scrdiag_state;
  struct scr_lease_key key;
  uint8_t *arena;
  uint64_t capacity;
  int ret;
  if (!dev || !output || output_size != 512) return -EINVAL;
  irqstate_t flags = enter_critical_section();
  if (!g_scrdiag_ready || dev != priv->dev || g_sector_attempted)
    { leave_critical_section(flags); return -EBUSY; }
  ret = mmcsd_sector_validate(priv, lba, &capacity);
  if (ret != OK) { leave_critical_section(flags); return ret; }
  g_sector_attempted = true;
  leave_critical_section(flags);
  ret = stm32_sector_claim(dev, lba, capacity, true, &arena, &key);
  if (ret != OK) return ret;
  SDIO_BLOCKSETUP(dev, 512, 1);
  SDIO_WAITENABLE(dev, SDIOWAIT_TRANSFERDONE | SDIOWAIT_TIMEOUT |
                      SDIOWAIT_ERROR, MMCSD_BLOCK_RDATADELAY);
  ret = SDIO_DMARECVSETUP(dev, arena, 512);
  if (ret == OK) ret = mmcsd_sendcmdpoll(priv, MMCSD_CMD17, (uint32_t)lba);
  if (ret == OK) ret = mmcsd_recv_r1(priv, MMCSD_CMD17);
  if (ret == OK) ret = mmcsd_eventwait(priv, SDIOWAIT_TIMEOUT | SDIOWAIT_ERROR);
  /* The finish path seals failures before releasing caller ownership.
   * No generic cancel/reset or retry, even when the transfer failed early.
   */
  return stm32_sector_finish(dev, key, ret, output);
}
int main(void){void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);struct stm32_dev_s*p=m;uint8_t out[512];for(inject=0;inject<5;inject++){fresh(p);p->scr_success_seen=true;p->scrlease.state=SCR_LEASE_HANDOFF;g_scrdiag_state=(struct mmcsd_state_s){.dev=&p->dev,.type=12,.csd={1u<<30,9u<<16,0,0},.blocksize=512,.blockshift=9};g_scrdiag_ready=true;g_sector_attempted=false;memset(out,0xa5,512);int r=mmcsd_sector_readonce(&p->dev,1,out,512);assert(r==(inject==1||inject==3?-ETIMEDOUT:inject?-EIO:0));assert(out[0]==(inject?0xa5:0x69)&&out[511]==out[0]);assert(p->scrlease.state==(inject?SCR_LEASE_QUARANTINED:SCR_LEASE_RETIRED));assert(mmcsd_sector_readonce(&p->dev,1,out,512)==-EBUSY);if(inject){terminal(p,0);assert(out[0]==0xa5&&scr_closed(p));}}
/* First SCR ACTIVE media notification must prevent HANDOFF too. */
for(int ev=0;ev<2;ev++){fresh(p);uint8_t*a;struct scr_lease_key k;assert(stm32_scr_claim(&p->dev,&a,&k)==0);assert(!p->scr_success_seen);if(ev)sdio_wrprotect(&p->dev,true);else sdio_mediachange(&p->dev,false);assert(p->scrlease.state==SCR_LEASE_QUARANTINED);}
/* Initial presence only; every received later notification invalidates IDLE. */
for(int ev=0;ev<4;ev++){fresh(p);if(ev!=0)sdio_mediachange(&p->dev,true);if(ev==0||ev==1)sdio_mediachange(&p->dev,false);else if(ev==2)sdio_mediachange(&p->dev,true);else sdio_wrprotect(&p->dev,false);assert(p->scrlease.state==SCR_LEASE_RETIRED);sdio_mediachange(&p->dev,true);uint8_t*a;struct scr_lease_key k;assert(stm32_scr_claim(&p->dev,&a,&k)==-EBUSY);}
munmap(m,4096);puts("PASS actual upper validate/readonce -> actual lower claim/setup/send/R1/ISR/endwait/finish normal, poll timeout, R1 error, data timeout, IDMA error; delayed DATAEND/no retry/no copy. Block/wait arm, poll timing/event dispatch and MMIO mocked; no hardware stop proof.");}
