from pathlib import Path
import subprocess
p=Path(__file__).resolve().parent
root=p.parent/'eventwait-error-seal-431fd0ea/product';f=root/'tools/hosttests/scr_sector'
s=(p.parent/'recover-seal-callgraph-be86b519/connected.c').read_text().split('int main(void)')[0]
a=s.index('static void notify_seal(');z=s.index('static void pthread_mutex_inconsistent',a)
s=s[:a]+'''
/* Candidate binding has static lifetime and immutable device. */
struct binding {
  struct notice notice;
  struct stm32_dev_s *const dev;
  struct scr_lease_key key;
};
static struct binding binding = {.dev=(void*)0x24000000};
static void notify_seal(struct notice*n,uint64_t generation){
  struct binding*b=(struct binding*)((char*)n - offsetof(struct binding,notice));
  assert(!n->owner && depth);
  if (generation == b->key.generation &&
      scr_lease_matches(&b->dev->scrlease,b->key) &&
      b->dev->scrlease.state == SCR_LEASE_ACTIVE)
    scr_seal(b->dev,-ECANCELED);
  notice_count++;
}
static int bind_lease(struct binding*b,struct tcb_s*owner,struct scr_lease_key key){
  assert(depth);
  if (!owner || key.owner != owner->id ||
      !scr_lease_matches(&b->dev->scrlease,key) ||
      b->dev->scrlease.state != SCR_LEASE_ACTIVE) return -ESTALE;
  /* bind first; failure must not replace the old immutable callback key. */
  int ret=bind_notice(&b->notice,owner,key.generation,notify_seal);
  if (ret==0) b->key=key;
  return ret;
}
'''+s[z:]
s+='''
int main(void){
void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);target=m;
struct tcb_s owner={1},wrongowner={2};
for(unsigned mismatch=0;mismatch<4;mismatch++){
fresh(target);struct scr_lease_key key=begin(target);setup(target);command(target);
head=NULL;dispatching=false;notice_count=cleanup_count=timer_fired=0;
unsigned flags=enter_critical_section();
assert(bind_lease(&binding,&wrongowner,key)==-ESTALE && head==NULL);
assert(bind_lease(&binding,&owner,key)==0);
struct scr_lease_key saved=binding.key;
assert(bind_lease(&binding,&owner,key)==-EBUSY);
assert(binding.key.instance==saved.instance && binding.key.owner==saved.owner && binding.key.generation==saved.generation);
/* Deliberately seed stale identity; these are admission tests, not legit reuse. */
if(mismatch==1)target->scrlease.owner++;
if(mismatch==2)target->scrlease.generation++;
if(mismatch==3)binding.key.instance=NULL;
nxtask_recover(&owner);leave_critical_section(flags);
assert(notice_count==1 && cleanup_count==4 && depth==0 && head==NULL);
assert(target->scrlease.state==(mismatch?SCR_LEASE_ACTIVE:SCR_LEASE_QUARANTINED));
assert(regs[STM32_SDMMC_IDMACTRLR_OFFSET]==STM32_SDMMC_IDMACTRLR_IDMAEN);
if(!mismatch){uint8_t out[512];memset(out,0xa5,sizeof out);terminal(target,0);assert(stm32_sector_finish(&target->dev,key,0,out)==-ECANCELED);for(unsigned j=0;j<512;j++)assert(out[j]==0xa5);}
}
munmap(m,4096);puts("PASS static device/full lease key binding: stale owner/generation/instance rejected, duplicate registration preserves token; host only");}
'''
(p/'connected.c').write_text(s)
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(f),'-I'+str(f/'stubs'),'-I'+str(root/'include/nuttx'),str(p/'connected.c'),'-o',str(p/'connected')],check=True)
subprocess.run([str(p/'connected')],check=True)
