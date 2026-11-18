#include "recovery-split.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
/* Diagnostic-only single-controller slot: no caller-supplied callback. */

static struct notice *head;
static bool dispatching;
extern struct notice *sd_recovery_node(void);
extern void sd_recovery_notify(uint64_t generation);
int bind_notice(struct notice*n,struct tcb_s*owner,uint64_t gen){
 assert(recovery_lock_held());
 if(n!=sd_recovery_node() || !owner || !gen)return -EINVAL;
 if(dispatching || head)return -EBUSY;
 n->owner=owner;n->generation=gen;head=n;return 0;
}
int unbind_notice(struct notice*n,struct tcb_s*owner,uint64_t gen){
 assert(recovery_lock_held());
 if(n!=sd_recovery_node() || head!=n || n->owner!=owner || n->generation!=gen)return -ESTALE;
 head=NULL;n->owner=NULL;return 0;
}
void recover_notices(struct tcb_s*owner){
 assert(recovery_lock_held());
 if(dispatching){assert(head==NULL);return;}
 if(!head || head->owner!=owner)return;
 uint64_t generation=head->generation;
 head->owner=NULL;head=NULL;
 dispatching=true;sd_recovery_notify(generation);dispatching=false;
}

#ifdef HOST_RECOVERY_TEST
struct notice **recovery_slot_for_test(void){return &head;}
bool *recovery_dispatch_for_test(void){return &dispatching;}
#endif
