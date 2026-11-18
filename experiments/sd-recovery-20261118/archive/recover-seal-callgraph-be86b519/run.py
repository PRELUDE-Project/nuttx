from pathlib import Path
import subprocess
p=Path(__file__).resolve().parent
r=p.parent/'eventwait-error-seal-431fd0ea/product'
f=r/'tools/hosttests/scr_sector'
s=(f/'driver.c').read_text().split('int main(void)')[0]
pre='''
struct test_node { bool linked; bool head; };
struct wdog_s { struct test_node node; void (*func)(void); unsigned expired; };
'''
s=s.replace('#define WDOG_ISACTIVE(w) (*(w))', '')
s=s.replace('struct stm32_dev_s{',pre+'struct stm32_dev_s{',1)
s=s.replace('bool fifowdog,waitwdog,unaligned_rx;', 'struct wdog_s fifowdog,waitwdog;bool unaligned_rx;')
old='static int wd_cancel(bool*w){*w=false;return 0;}'
assert s.count(old)==1
support='''
static unsigned g_wdspinlock, g_wdactivelist;
static unsigned spin_lock_irqsave(unsigned *p){(void)p;return enter_critical_section();}
static void spin_unlock_irqrestore(unsigned*p,unsigned f){(void)p;leave_critical_section(f);}
#define WDOG_ISACTIVE(w) ((w)->func != NULL)
#define sched_note_wdog(...) ((void)0)
#define nxsched_reassess_timer() ((void)0)
static bool list_is_head(unsigned*p,struct test_node*n){(void)p;return n->head;}
static void list_delete(struct test_node*n){assert(n->linked);n->linked=false;}
'''
wd=(p/'wd_cancel.c').read_text(); wd=wd[wd.index('int wd_cancel('):].replace('FAR ', '')
s=s.replace(old,support+wd)
registry=(p.parent/'recover-registration-be86b519/prototype.c').read_text()
registry=registry[registry.index('struct tcb_s'):registry.index('static unsigned order,')]
registry=registry.replace('static unsigned critical_depth;', '#define critical_depth depth')
s+=registry
s+='''
static struct stm32_dev_s *target;
static unsigned timer_fired, notice_count, cleanup_count;
static void timer_callback(void){timer_fired++;}
static void notify_seal(struct notice*n,uint64_t generation){
  assert(!n->owner && depth);
  if (generation == target->scrkey.generation && target->scrlease.state == SCR_LEASE_ACTIVE)
    scr_seal(target,-ECANCELED);
  notice_count++;
}
static void pthread_mutex_inconsistent(struct tcb_s*t){(void)t;cleanup_count++;}
static void wd_recover(struct tcb_s*t){(void)t;cleanup_count++;}
static void nxsem_recover(struct tcb_s*t){(void)t;cleanup_count++;}
static void nxmq_recover(struct tcb_s*t){(void)t;cleanup_count++;}
'''
s+=(p.parent/'recover-registration-be86b519/recover-body.inc').read_text()
s+='''
int main(void){
void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);
target=m;struct tcb_s owner={1};struct notice node={0};
for(unsigned active=0;active<2;active++)for(unsigned first=0;first<2;first++){
fresh(target);struct scr_lease_key key=begin(target);setup(target);command(target);
head=NULL;dispatching=false;notice_count=cleanup_count=timer_fired=0;
target->waitwdog.node.linked=active;target->waitwdog.node.head=first;target->waitwdog.func=active?timer_callback:NULL;
unsigned flags=enter_critical_section();assert(bind_notice(&node,&owner,key.generation,notify_seal)==0);
nxtask_recover(&owner);leave_critical_section(flags);
assert(depth==0 && notice_count==1 && cleanup_count==4 && timer_fired==0);
assert(!target->waitwdog.func && !target->waitwdog.node.linked);
assert(target->scrlease.state==SCR_LEASE_QUARANTINED && target->scrlease.result==-ECANCELED);
assert(!target->buffer && !target->remaining && !target->waitmask && !target->xfrmask);
assert(regs[STM32_SDMMC_IDMACTRLR_OFFSET]==STM32_SDMMC_IDMACTRLR_IDMAEN);
uint8_t output[512];memset(output,0xa5,sizeof output);terminal(target,0);
assert(stm32_sector_finish(&target->dev,key,0,output)==-ECANCELED);
for(unsigned i=0;i<512;i++)assert(output[i]==0xa5);
}
munmap(m,4096);puts("PASS actual recover+seal+wd_cancel connection: inactive/active timer, head/nonhead; no callback, no IDMA release. Host doubles, not OS/hardware.");}
'''
(p/'connected.c').write_text(s)
cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(f),'-I'+str(f/'stubs'),'-I'+str(r/'include/nuttx'),str(p/'connected.c'),'-o',str(p/'connected')]
subprocess.run(cmd,check=True)
subprocess.run([str(p/'connected')],check=True)
