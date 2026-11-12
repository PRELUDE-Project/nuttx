/* SPDX-License-Identifier: Apache-2.0 */
/* Extracted Apache NuttX functions with host-only test doubles. */
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef int irqstate_t;
struct sdio_dev_s {int x;};static struct sdio_dev_s dev;
static int inits,media,scr,reads,depth,mode; static uint64_t want;
static int enter_critical_section(void){return depth++;}
static void leave_critical_section(int f){assert(--depth==f);}
struct sdio_dev_s *sdio_initialize(int slot){assert(slot==0);inits++;return mode==4?NULL:&dev;}
void sdio_mediachange(struct sdio_dev_s*d,bool b){assert(d==&dev&&b);media++;}
int mmcsd_scr_diagnostic(struct sdio_dev_s*d){assert(d==&dev);scr++;return mode==2?-5:0;}
int mmcsd_sector_readonce(struct sdio_dev_s*d,uint64_t l,uint8_t*out,size_t n){assert(d==&dev&&l==want&&out&&n==512);reads++;return mode==3?-110:0;}
/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
extern struct sdio_dev_s *sdio_initialize(int slotno);
extern void sdio_mediachange(struct sdio_dev_s *dev, bool present);
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
/* Deliberately no signs, whitespace, prefixes or partial conversions. */
static bool sector_lba(const char *text, uint32_t *value)
{
  uint32_t n = 0;
  if (text == NULL || *text == '\0') return false;
  for (; *text != '\0'; text++)
    {
      if (*text < '0' || *text > '9') return false;
      uint32_t digit = (uint32_t)(*text - '0');
      if (n > (UINT32_MAX - digit) / 10u) return false;
      n = n * 10u + digit;
    }
  *value = n;
  return true;
}
#endif
/* H753 board-adapter hooks; not portable to a different SDIO controller. */
int scrdiag_main(int argc, char **argv)
{
  static bool started;
  bool sector = false;
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
  uint32_t lba = 0;
  /* CPU destination only: the driver owns its separate persistent DMA arena.
   * No sector bytes or checksum are exposed by this diagnostic.
   */
  static uint8_t output[512];
  if (argc == 3 && argv != NULL && argv[1] != NULL &&
      strcmp(argv[1], "sector") == 0 && sector_lba(argv[2], &lba))
    sector = true;
  else
#endif
  if (argc != 1) return 1;
#ifndef CONFIG_SD_SECTOR_READ_REVIEW
  (void)argv;
#endif
  /* Parse first: invalid requests must not consume the attempt or touch SDIO. */
  irqstate_t f = enter_critical_section();
  if (started) { leave_critical_section(f); return 1; }
  started = true;
  leave_critical_section(f);
  struct sdio_dev_s *dev = sdio_initialize(0);
  if (dev == NULL) return 1;
  /* Exactly one initial assumed-present notification. The operator must
   * verify the card and exclusive ownership; there is no hotplug detector.
   */
  sdio_mediachange(dev, true);
  int ret = mmcsd_scr_diagnostic(dev);
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
  if (sector && ret == 0)
    ret = mmcsd_sector_readonce(dev, lba, output, sizeof(output));
#endif
  if (sector)
    printf("Single-sector diagnostic result %d; no data displayed; no stop proof; no retry\n", ret);
  else
    printf("SCR-only diagnostic result %d; no sector test or stop proof; no retry\n", ret);
  return ret == 0 ? 0 : 1;
}
int main(int argc,char**argv){assert(argc==2);mode=atoi(argv[1]);char *valid[]={"scrdiag","sector","4294967295"};want=UINT32_MAX;
char*bad[]={"","-1","+1"," 1","1 ","0x10","1a","4294967296","999999999999999999999999"};
for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++){char*a[]={"scrdiag","sector",bad[i]};assert(scrdiag_main(3,a)==1&&inits==0);}
char *wrong[]={"scrdiag","read","1"};assert(scrdiag_main(3,wrong)==1&&inits==0);assert(scrdiag_main(2,valid)==1&&inits==0);assert(scrdiag_main(0,NULL)==1&&inits==0);
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
uint32_t parsed=99;assert(sector_lba("0",&parsed)&&parsed==0);assert(sector_lba("00001",&parsed)&&parsed==1);assert(!sector_lba(NULL,&parsed));
assert(scrdiag_main(3,NULL)==1&&inits==0);
if(mode==1){char*a[]={"scrdiag"};assert(scrdiag_main(1,a)==0&&reads==0);}
else {int r=scrdiag_main(3,valid);assert(r==((mode==2||mode==3||mode==4)?1:0));assert(reads==((mode==2||mode==4)?0:1));}
assert(inits==1&&media==(mode==4?0:1)&&scr==(mode==4?0:1));assert(scrdiag_main(3,valid)==1&&inits==1);
#else
assert(scrdiag_main(3,valid)==1&&inits==0);char*a[]={"scrdiag"};assert(scrdiag_main(1,a)==0&&inits==1&&scr==1&&reads==0);assert(scrdiag_main(1,a)==1&&inits==1);
#endif
assert(depth==0);return 0;}
