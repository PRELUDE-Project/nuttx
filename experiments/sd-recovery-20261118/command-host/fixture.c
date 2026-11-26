#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int scrdiag_main(int, char **);
struct sdio_dev_s { int unused; } dev;
static unsigned irq, init, notice, scr, read_count;
static uint32_t seen;
static bool fail;
unsigned enter_critical_section(void) { return ++irq; }
void leave_critical_section(unsigned state) { assert(state == irq); }
struct sdio_dev_s *sdio_initialize(int slot) { assert(slot == 0); ++init; return &dev; }
void sdio_mediachange(struct sdio_dev_s *p, bool present) { assert(p == &dev && present); ++notice; }
int mmcsd_scr_diagnostic(struct sdio_dev_s *p) { assert(p == &dev); ++scr; return fail ? -5 : 0; }
int mmcsd_sector_readonce(struct sdio_dev_s *p,uint32_t lba,uint8_t *out,size_t count) {
  assert(p == &dev && count == 512); ++read_count; seen = lba; memset(out, 0, count); return 0;
}
int main(int argc,char **argv) {
  assert(argc == 2);
  bool sector = strcmp(argv[1],"scr") != 0;
  fail = strcmp(argv[1],"fail") == 0;
  char *bad[] = {"-1", "+0", "0x1", "", " 1", "1x", "4294967296", "18446744073709551616", NULL};
  for (size_t i=0;i<sizeof bad/sizeof bad[0];i++) {
    char *a[]={"scrdiag","sector",bad[i],NULL};
    assert(scrdiag_main(3,a) == 1);
  }
  char *unknown[]={"scrdiag","write","0",NULL};
  assert(scrdiag_main(3,unknown) == 1);
  assert(irq == 0 && init == 0 && notice == 0 && scr == 0 && read_count == 0);
  char *a[]={"scrdiag","sector","4294967295",NULL};
  int n = sector ? 3 : 1;
#ifndef CONFIG_SD_SECTOR_READ_REVIEW
  assert(scrdiag_main(3,a) == 1 && irq == 0);
  n = 1;
  sector = false;
#endif
  assert(scrdiag_main(n,a) == (fail ? 1 : 0));
  assert(init == 1 && notice == 1 && scr == 1 && read_count == (unsigned)(sector && !fail));
  if (read_count) assert(seen == UINT32_MAX);
  assert(scrdiag_main(n,a) == 1 && init == 1 && notice == 1 && scr == 1);
  puts("PASS");
}
