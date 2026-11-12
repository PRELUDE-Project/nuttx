/* SPDX-License-Identifier: Apache-2.0 */
#include <nuttx/config.h>
#include <nuttx/irq.h>
#include <nuttx/sdio.h>
#include <nuttx/mmcsd_scrdiag.h>
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
