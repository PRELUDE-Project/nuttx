/* Candidate only: caller TCB lookup and lease mutation share one critical. */
struct binding {
  struct nxsched_sd_notice notice;
  struct stm32_dev_s *const dev;
  struct scr_lease_key key;
};
static struct binding binding = {.dev=&g_sdmmcdev1};
struct nxsched_sd_notice *sd_recovery_node(void){return &binding.notice;}
void sd_recovery_notify(uint64_t generation){
  if(generation==binding.key.generation &&
     scr_lease_matches(&binding.dev->scrlease,binding.key) &&
     binding.dev->scrlease.state==SCR_LEASE_ACTIVE)
    scr_seal(binding.dev,-ECANCELED);
}
static int bind_lease(struct binding*b,struct tcb_s*owner,struct scr_lease_key key){
  if (!owner || key.owner != owner->pid ||
      !scr_lease_matches(&b->dev->scrlease,key) ||
      b->dev->scrlease.state != SCR_LEASE_ACTIVE) return -ESTALE;
  /* bind first; failure must not replace the old immutable callback key. */
  int ret=nxsched_sd_bind_notice(&b->notice,owner,key.generation);
  if (ret==0) b->key=key;
  return ret;
}
static int atomic_finish(struct tcb_s*owner,struct scr_lease_key key,int error,void*out){
  irqstate_t flags=enter_critical_section();
  if(!owner || key.owner!=owner->pid || key.instance!=binding.key.instance ||
     key.owner!=binding.key.owner || key.generation!=binding.key.generation ||
     (binding.notice.owner && binding.notice.owner!=owner) ||
     (!binding.notice.owner && binding.dev->scrlease.state==SCR_LEASE_ACTIVE)){
    leave_critical_section(flags);return -ESTALE;
  }

  if(binding.notice.owner){
    if(nxsched_sd_validate_notice(&binding.notice,owner,key.generation)<0){
      scr_seal(binding.dev,-EIO);leave_critical_section(flags);return -EIO;
    }
  }
  /* Private binding is outside the controller object checked by raw finish. */
  uintptr_t dst=(uintptr_t)out, base=(uintptr_t)&binding;
  size_t bytes=binding.dev->sector_mode?512:8;
  if(error>=0 && binding.dev->scrlease.state==SCR_LEASE_ACTIVE && out &&
     dst<=UINTPTR_MAX-(bytes-1) && dst<=base+sizeof(binding)-1 &&
     base<=dst+bytes-1){
    scr_seal(binding.dev,-EINVAL);leave_critical_section(flags);return -EINVAL;
  }
  int ret=binding.dev->sector_mode?raw_stm32_sector_finish(&binding.dev->dev,key,error,out):raw_stm32_scr_finish(&binding.dev->dev,key,error,out);
  if(binding.notice.owner && binding.dev->scrlease.state!=SCR_LEASE_ACTIVE && scr_lease_matches(&binding.dev->scrlease,key)){
    int u=nxsched_sd_unbind_notice(&binding.notice,owner,key.generation);
    if(u<0){scr_seal(binding.dev,-EIO);ret=-EIO;}
  }
  leave_critical_section(flags);return ret;
}
static int scoped_finish(struct scr_lease_key key,int error,void*out){
 irqstate_t flags=enter_critical_section();
 struct tcb_s*owner=nxsched_get_tcb(nxsched_gettid());
 int ret=owner?atomic_finish(owner,key,error,out):-ESRCH;
 leave_critical_section(flags);return ret;
}
static int registered_claim(struct sdio_dev_s*dev,bool sector,uint64_t lba,uint64_t capacity,bool block,uint8_t**arena,struct scr_lease_key*key){
 if(!arena || !key || dev!=&binding.dev->dev)return -EINVAL;
 if(sector && (!block || capacity==0 || capacity>(uint64_t)UINT32_MAX+1 || lba>=capacity || lba>UINT32_MAX))return -EINVAL;
 irqstate_t flags=enter_critical_section();
 struct tcb_s*owner=nxsched_get_tcb(nxsched_gettid());
 int ret=-ESRCH;uint8_t*a;struct scr_lease_key k;
 if(owner){
  ret=sector?raw_stm32_sector_claim(dev,lba,capacity,block,&a,&k):raw_stm32_scr_claim(dev,&a,&k);
  if(ret==0){ret=bind_lease(&binding,owner,k);if(ret<0)scr_seal(binding.dev,ret);else{*arena=a;*key=k;}}
 }
 leave_critical_section(flags);return ret;
}
int stm32_scr_claim(struct sdio_dev_s*dev,uint8_t**arena,struct scr_lease_key*key){return registered_claim(dev,false,0,0,false,arena,key);}
int stm32_sector_claim(struct sdio_dev_s*dev,uint64_t lba,uint64_t capacity,bool block,uint8_t**arena,struct scr_lease_key*key){return registered_claim(dev,true,lba,capacity,block,arena,key);}
int stm32_scr_finish(struct sdio_dev_s*dev,struct scr_lease_key key,int error,uint32_t out[2]){irqstate_t flags=enter_critical_section();int ret=(dev!=&binding.dev->dev || binding.dev->sector_mode)?-EINVAL:scoped_finish(key,error,out);leave_critical_section(flags);return ret;}
int stm32_sector_finish(struct sdio_dev_s*dev,struct scr_lease_key key,int error,uint8_t out[512]){irqstate_t flags=enter_critical_section();int ret=(dev!=&binding.dev->dev || !binding.dev->sector_mode)?-EINVAL:scoped_finish(key,error,out);leave_critical_section(flags);return ret;}
