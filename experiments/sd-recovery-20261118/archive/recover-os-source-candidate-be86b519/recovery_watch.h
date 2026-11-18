/* SPDX-License-Identifier: Apache-2.0 */
#ifndef __INCLUDE_NUTTX_RECOVERY_WATCH_H
#define __INCLUDE_NUTTX_RECOVERY_WATCH_H
#include <nuttx/config.h>
#include <stdint.h>
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
#if defined(CONFIG_SMP) || defined(CONFIG_ARCH_HIPRI_INTERRUPT) || \
    defined(CONFIG_SCHED_TICKLESS) || defined(CONFIG_SCHED_INSTRUMENTATION) || \
    defined(CONFIG_IRQCOUNT)
#  error "Experimental recovery watch: unsupported synchronization/callback configuration"
#endif
/* Experimental fixed-diagnostic API. NOT a general task-exit callback API.
 * Nodes have static lifetime. Caller must hold an outer critical section
 * from TCB lookup through bind/unbind. Callback runs synchronously under
 * recovery exclusion: no task lifecycle calls, blocking, allocation, or bind.
 * This draft still requires configuration and reentrancy enforcement.
 */
struct tcb_s;
struct nxsched_recovery_watch_s {
  struct nxsched_recovery_watch_s *next;
  struct tcb_s *owner;
  uint64_t generation;
  void (*notify)(struct nxsched_recovery_watch_s *, uint64_t);
};

int nxsched_recovery_bind_locked(struct nxsched_recovery_watch_s *,struct tcb_s *,uint64_t,void (*)(struct nxsched_recovery_watch_s *,uint64_t));
int nxsched_recovery_unbind_locked(struct nxsched_recovery_watch_s *,struct tcb_s *,uint64_t);
#endif
#endif
