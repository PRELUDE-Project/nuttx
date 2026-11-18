#ifndef __INCLUDE_NUTTX_SD_RECOVERY_NOTICE_H
#define __INCLUDE_NUTTX_SD_RECOVERY_NOTICE_H
#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>
struct tcb_s;
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
#if !defined(CONFIG_ARCH_CHIP_STM32H7) || !defined(CONFIG_STM32H7_SDMMC1) || !defined(CONFIG_STM32H7_SDMMC_IDMA) || !defined(CONFIG_SD_SCR_CONTAINMENT_REVIEW) || !defined(CONFIG_SD_RECEIVE_LEASE_ONLY_REVIEW)
#error "Missing required fixed diagnostic driver configuration"
#endif
#if defined(CONFIG_STM32H7_SDMMC2) || defined(CONFIG_SMP) || defined(CONFIG_ARCH_HIPRI_INTERRUPT) || defined(CONFIG_SCHED_TICKLESS) || defined(CONFIG_SCHED_INSTRUMENTATION) || defined(CONFIG_IRQCOUNT)
#error "Unsupported fixed diagnostic recovery configuration"
#endif

struct nxsched_sd_notice {struct tcb_s *owner;uint64_t generation;};
int nxsched_sd_bind_notice(struct nxsched_sd_notice*,struct tcb_s*,uint64_t);
int nxsched_sd_unbind_notice(struct nxsched_sd_notice*,struct tcb_s*,uint64_t);
/* Caller must keep lookup/validate/copy/unbind in one outer critical section. */
int nxsched_sd_validate_notice(struct nxsched_sd_notice*,struct tcb_s*,uint64_t);
void nxsched_sd_recover_notices(struct tcb_s*);
#ifdef HOST_RECOVERY_TEST
struct nxsched_sd_notice **recovery_slot_for_test(void);
bool *recovery_dispatch_for_test(void);
#endif
#else
/* No diagnostic exists: do not introduce a driver link dependency. */
static inline void nxsched_sd_recover_notices(struct tcb_s *owner){(void)owner;}
#endif
#endif
