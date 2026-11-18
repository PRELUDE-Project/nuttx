#include <nuttx/sd_recovery_notice.h>
#ifdef CONFIG_SD_SECTOR_READ_REVIEW
#include <debug.h>
#include <nuttx/irq.h>
#include <errno.h>
#include <stddef.h>
/* Diagnostic-only single-controller slot: no caller-supplied callback. */

static struct nxsched_sd_notice *head;
static bool dispatching;
extern struct nxsched_sd_notice *sd_recovery_node(void);
extern void sd_recovery_notify(uint64_t generation);
static int bind_notice_locked(struct nxsched_sd_notice*n,struct tcb_s*owner,uint64_t gen){
 if(n!=sd_recovery_node() || !owner || !gen)return -EINVAL;
 if(dispatching || head)return -EBUSY;
 n->owner=owner;n->generation=gen;head=n;return 0;
}
static int unbind_notice_locked(struct nxsched_sd_notice*n,struct tcb_s*owner,uint64_t gen){
 if(n!=sd_recovery_node() || head!=n || n->owner!=owner || n->generation!=gen)return -ESTALE;
 head=NULL;n->owner=NULL;return 0;
}
/* These internal locks protect the registry, not the caller's TCB lookup.
 * Driver lookup/claim/bind and finish/unbind must retain one outer critical.
 */
int nxsched_sd_bind_notice(struct nxsched_sd_notice *n, struct tcb_s *owner, uint64_t generation)
{
 irqstate_t flags = enter_critical_section();
 int ret = bind_notice_locked(n, owner, generation);
 leave_critical_section(flags);
 return ret;
}
int nxsched_sd_unbind_notice(struct nxsched_sd_notice *n, struct tcb_s *owner, uint64_t generation)
{
 irqstate_t flags = enter_critical_section();
 int ret = unbind_notice_locked(n, owner, generation);
 leave_critical_section(flags);
 return ret;
}
/* Read-only pre-copy check. Caller must hold its outer lookup/finish critical. */
int nxsched_sd_validate_notice(struct nxsched_sd_notice *n, struct tcb_s *owner, uint64_t generation)
{
 irqstate_t flags = enter_critical_section();
 int ret = (n == sd_recovery_node() && head == n && owner != NULL &&
            n->owner == owner && n->generation == generation) ? 0 : -ESTALE;
 leave_critical_section(flags);
 return ret;
}
static void recover_notices_locked(struct tcb_s*owner){
 if(dispatching){DEBUGASSERT(head==NULL);return;}
 if(!head || head->owner!=owner)return;
 uint64_t generation=head->generation;
 head->owner=NULL;head=NULL;
 dispatching=true;sd_recovery_notify(generation);dispatching=false;
}

void nxsched_sd_recover_notices(struct tcb_s *owner)
{
 irqstate_t flags = enter_critical_section();
 recover_notices_locked(owner);
 leave_critical_section(flags);
}

#ifdef HOST_RECOVERY_TEST
struct nxsched_sd_notice **recovery_slot_for_test(void){return &head;}
bool *recovery_dispatch_for_test(void){return &dispatching;}
#endif

#endif
