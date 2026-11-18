from pathlib import Path
import subprocess
p=Path(__file__).resolve().parent;r=p.parent/'eventwait-error-seal-431fd0ea/product';f=r/'tools/hosttests/scr_sector'
s=(p.parent/'recover-key-binding-be86b519/connected.c').read_text().split('int main(void)')[0]
s+='''
/* Unpublished integration sketch. All callees must use this same exclusion. */
static int atomic_claim(struct tcb_s*owner,bool sector,uint8_t**arena,struct scr_lease_key*key){
  uint8_t *a; struct scr_lease_key k;
  if(!arena || !key || !owner)return -EINVAL;
  unsigned flags=enter_critical_section();
  int ret=sector?stm32_sector_claim(&binding.dev->dev,1,4096,true,&a,&k):stm32_scr_claim(&binding.dev->dev,&a,&k);
  if(ret==0){
    assert(depth && scr_lease_matches(&binding.dev->scrlease,binding.dev->scrkey));
    ret=bind_lease(&binding,owner,k);
    if(ret<0) scr_seal(binding.dev,ret);
    else {*arena=a;*key=k;}
  }
  leave_critical_section(flags);return ret;
}
static int atomic_finish(struct tcb_s*owner,struct scr_lease_key key,int error,void*out){
  unsigned flags=enter_critical_section();
  if(!owner || key.owner!=owner->id || key.instance!=binding.key.instance ||
     key.owner!=binding.key.owner || key.generation!=binding.key.generation ||
     (binding.notice.owner && binding.notice.owner!=owner) ||
     (!binding.notice.owner && binding.dev->scrlease.state==SCR_LEASE_ACTIVE)){
    leave_critical_section(flags);return -ESTALE;
  }
  int ret=binding.dev->sector_mode?stm32_sector_finish(&binding.dev->dev,key,error,out):stm32_scr_finish(&binding.dev->dev,key,error,out);
  if(binding.notice.owner && binding.dev->scrlease.state!=SCR_LEASE_ACTIVE && scr_lease_matches(&binding.dev->scrlease,key)){
    int u=unbind_notice(&binding.notice,owner,key.generation);
    if(u<0){scr_seal(binding.dev,-EIO);ret=-EIO;}
  }
  leave_critical_section(flags);return ret;
}
static void complete_scr(void){
 struct stm32_dev_s*p=binding.dev;uint32_t r;
 p->blocksize=8;assert(stm32_dmarecvsetup(&p->dev,p->scrlease.arena,8)==0);
 assert(stm32_sendcmd(&p->dev,SD_CMD55,0x10000)==0);regs[9]=55;regs[10]=0;regs[0]=0;
 assert(stm32_recvshortcrc(&p->dev,SD_CMD55,&r)==0);assert(stm32_sendcmd(&p->dev,SD_ACMD51,0)==0);
 regs[9]=51;assert(stm32_recvshortcrc(&p->dev,SD_ACMD51,&r)==0);
 p->waitevents=SDIOWAIT_TRANSFERDONE;regs[0]=STM32_SDMMC_STA_DATAEND;regs[6]=0;regs[7]=8;
 regs[4]=STM32_SDMMC_DCTRL_DTDIR|STM32_SDMMC_DCTRL_8BYTES;
 assert(stm32_sdmmc_interrupt(0,NULL,p)==0);
}
int main(void){
 void*m=mmap((void*)0x24000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(m!=MAP_FAILED);target=m;
 struct tcb_s owner={1},next={2};uint8_t*a;struct scr_lease_key k;uint32_t scrout[2];
 fresh(target);head=NULL;dispatching=true;
 a=(void*)0x1234;memset(&k,0,sizeof k);
 assert(atomic_claim(&owner,false,&a,&k)==-EBUSY);
 assert(a==(void*)0x1234 && k.instance==NULL && target->scrlease.state==SCR_LEASE_QUARANTINED && head==NULL && depth==0);
 /* Separate clean boot model; never reset a real used controller. */
 fresh(target);head=NULL;dispatching=false;
 assert(atomic_claim(&owner,false,&a,&k)==0 && head==&binding.notice && depth==0);
 complete_scr();scrout[0]=scrout[1]=0xa5a5a5a5;
 assert(atomic_finish(NULL,k,0,scrout)==-ESTALE);
 assert(atomic_finish(&next,k,0,scrout)==-ESTALE);
 assert(scrout[0]==0xa5a5a5a5 && scrout[1]==0xa5a5a5a5 && head==&binding.notice && target->scrlease.state==SCR_LEASE_ACTIVE);
 assert(atomic_finish(&owner,k,0,scrout)==0 && head==NULL && target->scrlease.state==SCR_LEASE_HANDOFF);
 unsigned flags=enter_critical_section();nxtask_recover(&owner);leave_critical_section(flags);
 assert(target->scrlease.state==SCR_LEASE_HANDOFF);
 current_owner=2;assert(atomic_claim(&next,true,&a,&k)==0 && head==&binding.notice);
 flags=enter_critical_section();nxtask_recover(&owner);leave_critical_section(flags);
 assert(target->scrlease.state==SCR_LEASE_ACTIVE && head==&binding.notice);
 setup(target);command(target);
 flags=enter_critical_section();nxtask_recover(&next);leave_critical_section(flags);
 assert(target->scrlease.state==SCR_LEASE_QUARANTINED && head==NULL);
 uint8_t out[512];memset(out,0xa5,sizeof out);terminal(target,0);
 assert(atomic_finish(&next,k,0,out)==-ECANCELED);for(unsigned i=0;i<512;i++)assert(out[i]==0xa5);
 assert(depth==0 && regs[STM32_SDMMC_IDMACTRLR_OFFSET]==STM32_SDMMC_IDMACTRLR_IDMAEN);
 munmap(m,4096);puts("PASS proposed atomic claim/register/failure and actual SCR HANDOFF/unbind/sector transfer/new-owner seal; host only");}
'''
(p/'connected.c').write_text(s)
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(f),'-I'+str(f/'stubs'),'-I'+str(r/'include/nuttx'),str(p/'connected.c'),'-o',str(p/'connected')],check=True)
subprocess.run([str(p/'connected')],check=True)
