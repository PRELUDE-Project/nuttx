from pathlib import Path
import shutil
r=Path('sd-recovery-627d0ec');p=r/'recover-release-connected-be86b519';q=r/'recover-selflocked-be86b519'
for name in ['scheduler.c','recovery-split.h','stubs/nuttx/irq.h']:
 (p/name).parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(q/name,p/name)
source=(p/'sched_releasetcb-original.c').read_text();body=source[source.index('int nxsched_release_tcb('):]
s=(q/'unlocked.c').read_text();s=s[:s.index('int main(void)')]
s=s.replace('struct tcb_s { union {unsigned id; unsigned pid;}; };','struct tcb_s { union {unsigned id; unsigned pid;}; struct tcb_s *flink,*blink; void *stack_alloc_ptr; unsigned flags; };')
s+='''
#define CONFIG_DISABLE_PTHREAD 1
#define CONFIG_DISABLE_POSIX_TIMERS 1
#define FAR
#define TCB_FLAG_FREE_TCB 1
static unsigned releases, stale;
static void observe_release(void){
 releases++;
 if(head || target->scrlease.state!=SCR_LEASE_QUARANTINED)stale++;
}
static void nxsched_releasepid(pid_t pid){assert(pid==1);observe_release();}
static void up_release_stack(struct tcb_s*t,uint8_t type){(void)t;(void)type;observe_release();}
static void group_leave(struct tcb_s*t){(void)t;observe_release();}
static void kmm_free(void*t){(void)t;observe_release();}
'''
body=body.replace('      /* Released tcb', '''#ifdef PROPOSED_RELEASE_HOOK
      recover_notices(tcb);
#endif
      /* Released tcb''')
s+=body
s+='''
int main(void){
 void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);target=m;
 for(unsigned early=0;early<2;early++)for(unsigned outer=0;outer<2;outer++){
  struct tcb_s owner={.pid=1,.stack_alloc_ptr=(void*)0x1234,.flags=TCB_FLAG_FREE_TCB};
  uint8_t *arena;struct scr_lease_key key={0};
  fresh(target);head=NULL;dispatching=false;notice_count=0;releases=0;stale=0;table[1]=&owner;
  assert(stm32_scr_claim(&target->dev,&arena,&key)==0);
  unsigned saved=depth;if(outer)enter_critical_section();unsigned expected=depth;
  if(early)nxtask_recover(&owner);
  assert(nxsched_release_tcb(&owner,0)==0 && depth==expected && releases==4);
#ifdef PROPOSED_RELEASE_HOOK
  assert(stale==0 && notice_count==1 && !head);
#else
  assert(stale==(early?0:4) && notice_count==(early?1:0));
#endif
  if(outer){leave_critical_section(saved);}assert(depth==0);
 }
 assert(nxsched_release_tcb(NULL,0)==0 && depth==0);
 munmap(m,4096);puts("PASS real release body: direct/early recover x outer/no lock, PID/stack/group/TCB observation; host only");
}
'''
(p/'connected.c').write_text(s)
c=(q/'check.py').read_text().replace("for case in ['nested','unlocked']:","for case in ['baseline','candidate']:").replace("str(p/(case+'.c'))","str(p/'connected.c')").replace("['cc',*flags,*defs(required)","['cc',*(['-DPROPOSED_RELEASE_HOOK'] if case=='candidate' else []),*flags,*defs(required)")
(p/'check.py').write_text(c)
