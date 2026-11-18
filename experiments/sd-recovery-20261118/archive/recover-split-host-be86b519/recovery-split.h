#ifndef HOST_RECOVERY_SPLIT_H
#define HOST_RECOVERY_SPLIT_H
#include <stdint.h>
#include <stdbool.h>
struct tcb_s;
struct notice {struct tcb_s *owner;uint64_t generation;};
int bind_notice(struct notice*,struct tcb_s*,uint64_t);
int unbind_notice(struct notice*,struct tcb_s*,uint64_t);
void recover_notices(struct tcb_s*);
bool recovery_lock_held(void);
#ifdef HOST_RECOVERY_TEST
struct notice **recovery_slot_for_test(void);
bool *recovery_dispatch_for_test(void);
#endif
#endif
