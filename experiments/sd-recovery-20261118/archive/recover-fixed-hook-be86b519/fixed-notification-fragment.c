struct notice {struct tcb_s *owner;uint64_t generation;};
static struct notice *head;
static bool dispatching;
static struct notice *sd_recovery_node(void);
static void sd_recovery_notify(uint64_t generation);
static int bind_notice(struct notice*n,struct tcb_s*owner,uint64_t gen){
 assert(depth);
 if(n!=sd_recovery_node() || !owner || !gen)return -EINVAL;
 if(dispatching || head)return -EBUSY;
 n->owner=owner;n->generation=gen;head=n;return 0;
}
static int unbind_notice(struct notice*n,struct tcb_s*owner,uint64_t gen){
 assert(depth);
 if(n!=sd_recovery_node() || head!=n || n->owner!=owner || n->generation!=gen)return -ESTALE;
 head=NULL;n->owner=NULL;return 0;
}
static void recover_notices(struct tcb_s*owner){
 assert(depth && !dispatching);
 if(!head || head->owner!=owner)return;
 uint64_t generation=head->generation;
 head->owner=NULL;head=NULL;
 dispatching=true;sd_recovery_notify(generation);dispatching=false;
}
