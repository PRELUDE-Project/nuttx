
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
struct stm32_dev_s{int dummy;};
#define STM32_SDMMC_POWER_OFFSET 0
#define STM32_SDMMC_CLKCR_OFFSET 1
#define STM32_SDMMC_DCTRL_OFFSET 2
#define STM32_SDMMC_DTIMER_OFFSET 3
#define STM32_SDMMC_DLEN_OFFSET 4
#define STM32_SDMMC_DCOUNT_OFFSET 5
#define STM32_SDMMC_STA_OFFSET 6
#define STM32_SDMMC_MASK_OFFSET 7
static unsigned seen,reads,state;static char output[2048];static unsigned pos;
static uint32_t sdmmc_getreg32(struct stm32_dev_s*p,int off){(void)p;assert(off!=5);seen|=1u<<off;reads++;return off==6?state:(unsigned)off;}
static void mcinfo(const char*fmt,...){va_list a;va_start(a,fmt);pos+=vsnprintf(output+pos,sizeof output-pos,fmt,a);va_end(a);}
struct stm32_sdioregs_s
{
  uint8_t  power;
  uint16_t clkcr;
  uint16_t dctrl;
  uint32_t dtimer;
  uint32_t dlen;
  uint32_t sta;
  uint32_t mask;
};static void stm32_sdiosample(struct stm32_dev_s *priv,
                             struct stm32_sdioregs_s *regs)
{
  regs->power   = (uint8_t)sdmmc_getreg32(priv, STM32_SDMMC_POWER_OFFSET);
  regs->clkcr   = (uint16_t)sdmmc_getreg32(priv, STM32_SDMMC_CLKCR_OFFSET);
  regs->dctrl   = (uint16_t)sdmmc_getreg32(priv, STM32_SDMMC_DCTRL_OFFSET);
  regs->dtimer  = sdmmc_getreg32(priv, STM32_SDMMC_DTIMER_OFFSET);
  regs->dlen    = sdmmc_getreg32(priv, STM32_SDMMC_DLEN_OFFSET);
  /* DCNTR is only specified after completion/hold, not arbitrary samples. */
  regs->sta     = sdmmc_getreg32(priv, STM32_SDMMC_STA_OFFSET);
  regs->mask    = sdmmc_getreg32(priv, STM32_SDMMC_MASK_OFFSET);
}static void stm32_sdiodump(struct stm32_sdioregs_s *regs, const char *msg)
{
  mcinfo("SDIO Registers: %s\n", msg);
  mcinfo("  POWER[%08x]: %08" PRIx8 "\n", STM32_SDMMC_POWER_OFFSET,
         regs->power);
  mcinfo("  CLKCR[%08x]: %08" PRIx16 "\n", STM32_SDMMC_CLKCR_OFFSET,
         regs->clkcr);
  mcinfo("  DCTRL[%08x]: %08" PRIx16 "\n", STM32_SDMMC_DCTRL_OFFSET,
         regs->dctrl);
  mcinfo(" DTIMER[%08x]: %08" PRIx32 "\n", STM32_SDMMC_DTIMER_OFFSET,
         regs->dtimer);
  mcinfo("   DLEN[%08x]: %08" PRIx32 "\n", STM32_SDMMC_DLEN_OFFSET,
         regs->dlen);
  mcinfo(" DCOUNT: omitted (sample may be active or failed)\n");
  mcinfo("    STA[%08x]: %08" PRIx32 "\n", STM32_SDMMC_STA_OFFSET,
         regs->sta);
  mcinfo("   MASK[%08x]: %08" PRIx32 "\n", STM32_SDMMC_MASK_OFFSET,
         regs->mask);
}
int main(void){struct stm32_dev_s p;struct stm32_sdioregs_s sample;
for(unsigned st=0;st<4;st++){state=st;seen=reads=pos=0;output[0]=0;stm32_sdiosample(&p,&sample);assert(reads==7&&seen==0xdf&&sample.sta==state);stm32_sdiodump(&sample,"test");assert(strstr(output,"DCOUNT: omitted")&&reads==7);}
puts("PASS extracted XFRDEBUG sample/dump: no DCOUNT read for any modeled state; seven remaining reads and explicit omitted label. MMIO mocked, no stop proof.");}
