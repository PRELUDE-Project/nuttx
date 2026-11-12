/* SPDX-License-Identifier: Apache-2.0 */
#ifndef __INCLUDE_NUTTX_MMCSD_SCRDIAG_H
#define __INCLUDE_NUTTX_MMCSD_SCRDIAG_H
#include <nuttx/config.h>
#include <nuttx/sdio.h>
#ifdef CONFIG_SD_RECEIVE_LEASE_ONLY_REVIEW
#ifdef __cplusplus
extern "C" {
#endif
/* Trusted, task-context, one-shot H753 diagnostic. Caller provides a freshly
 * initialized exclusive controller and has verified actual card presence.
 * Changes card protocol state. Never registers a block device, frees retained
 * state, retries, or grants physical-stop/recovery authority. Not a raw ioctl.
 */
int mmcsd_scr_diagnostic(struct sdio_dev_s *dev);
#ifdef __cplusplus
}
#endif
#endif
#endif
