from pathlib import Path
import subprocess
p=Path(__file__).resolve().parent;r=p.parent/'eventwait-error-seal-431fd0ea/product';f=r/'tools/hosttests/scr_sector'
s=(p.parent/'recover-atomic-binding-be86b519/connected.c').read_text().split('int main(void)')[0]
s=s.replace('struct tcb_s { unsigned id; };','struct tcb_s { union {unsigned id; unsigned pid;}; };')
a=(p/'sched_gettcb.c').read_text();a=a[a.index('FAR struct tcb_s *nxsched_get_tcb('):].replace('FAR ','')
s+='''
typedef int pid_t; /* host PID representation, not target ABI */
static struct tcb_s *table[4];static struct tcb_s **g_pidhash=table;
#define PIDHASH(pid) ((pid)&3)
'''+a+'''
static int scoped_claim(bool sector,uint8_t**arena,struct scr_lease_key*key){
 unsigned flags=enter_critical_section();
 struct tcb_s*owner=nxsched_get_tcb(nxsched_gettid());
 int ret=owner?atomic_claim(owner,sector,arena,key):-ESRCH;
 leave_critical_section(flags);return ret;
}
int main(void){
void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);target=m;
struct tcb_s owner={.pid=1},collision={.pid=5};uint8_t*a=(void*)0x1234;struct scr_lease_key k={0};
fresh(target);head=NULL;dispatching=false;
table[1]=&collision;assert(scoped_claim(false,&a,&k)==-ESRCH);
assert(target->scrlease.state==SCR_LEASE_IDLE && head==NULL && a==(void*)0x1234 && !k.instance && depth==0);
table[1]=&owner;assert(scoped_claim(false,&a,&k)==0);
assert(head==&binding.notice && binding.notice.owner==&owner && binding.key.owner==1 && depth==0);
unsigned flags=enter_critical_section();nxtask_recover(&owner);table[1]=NULL;leave_critical_section(flags);
assert(head==NULL && target->scrlease.state==SCR_LEASE_QUARANTINED);
/* Artificial numeric/TCB reuse after notification consumption. */
table[1]=&owner;assert(scoped_claim(false,&a,&k)==-EBUSY && head==NULL);
munmap(m,4096);puts("PASS actual TCB lookup within outer claim scope; hash collision rejected before claim; recovered lease rejects modeled PID reuse. Host not OS.");}
'''
(p/'connected.c').write_text(s)
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast','-Wno-sign-compare','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(f),'-I'+str(f/'stubs'),'-I'+str(r/'include/nuttx'),str(p/'connected.c'),'-o',str(p/'connected')],check=True)
subprocess.run([str(p/'connected')],check=True)
