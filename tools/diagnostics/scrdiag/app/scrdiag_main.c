/* SPDX-License-Identifier: Apache-2.0 */
#include <nuttx/config.h>
#include <nuttx/irq.h>
#include <nuttx/sdio.h>
#include <nuttx/mmcsd_scrdiag.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
extern struct sdio_dev_s *sdio_initialize(int slotno);
extern void sdio_mediachange(struct sdio_dev_s *dev, bool present);
/* H753 board-adapter hooks; not portable to a different SDIO controller. */
int scrdiag_main(int argc, char **argv)
{
  static bool started;
  (void)argv;
  if (argc != 1) return 1;
  irqstate_t f = enter_critical_section();
  if (started) { leave_critical_section(f); return 1; }
  started = true;
  leave_critical_section(f);
  struct sdio_dev_s *dev = sdio_initialize(0);
  if (dev == NULL) return 1;
  /* Manual invocation assumes the operator verified the physical card.
   * No callback is registered; never claim an actually detected card.
   */
  sdio_mediachange(dev, true);
  int ret = mmcsd_scr_diagnostic(dev);
  printf("SCR-only diagnostic result %d; no sector test or stop proof; no retry\n", ret);
  return ret == 0 ? 0 : 1;
}
