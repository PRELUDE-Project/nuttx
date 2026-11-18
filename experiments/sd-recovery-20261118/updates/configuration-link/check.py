from pathlib import Path
import subprocess,tempfile,json,argparse
parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);args=parser.parse_args()
p=Path(__file__).resolve().parent;r=args.repo.resolve();f=r/'tools/hosttests/scr_sector'
required=['CONFIG_SD_SECTOR_READ_REVIEW','CONFIG_ARCH_CHIP_STM32H7','CONFIG_STM32H7_SDMMC1','CONFIG_STM32H7_SDMMC_IDMA','CONFIG_SD_SCR_CONTAINMENT_REVIEW','CONFIG_SD_RECEIVE_LEASE_ONLY_REVIEW']
unsupported=['CONFIG_STM32H7_SDMMC2','CONFIG_SMP','CONFIG_ARCH_HIPRI_INTERRUPT','CONFIG_SCHED_TICKLESS','CONFIG_SCHED_INSTRUMENTATION','CONFIG_IRQCOUNT']
flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast','-Wno-sign-compare']
def defs(xs):return ['-D'+x+'=1' for x in xs]
with tempfile.TemporaryDirectory() as tmp:
 out=str(Path(tmp)/'test')
 subprocess.run(['cc',*flags,str(p/'scheduler.c'),str(p/'smoke.c'),'-o',out],check=True);subprocess.run([out],check=True)
 # Enabled mode must fail to link if the fixed driver hook is missing.
 link=subprocess.run(['cc',*flags,*defs(required),str(p/'scheduler.c'),str(p/'smoke.c'),'-o',out],capture_output=True,text=True)
 assert link.returncode and 'sd_recovery_node' in link.stderr and 'sd_recovery_notify' in link.stderr
 for bad in unsupported:
  c=subprocess.run(['cc','-E',*defs(required+[bad]),str(p/'smoke.c')],capture_output=True,text=True);assert c.returncode and 'Unsupported fixed' in c.stderr
 for missing in required[1:]:
  c=subprocess.run(['cc','-E',*defs([x for x in required if x!=missing]),str(p/'smoke.c')],capture_output=True,text=True);assert c.returncode and 'Missing required' in c.stderr
 subprocess.run(['cc',*flags,*defs(required),'-DHOST_RECOVERY_TEST','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(f),'-I'+str(f/'stubs'),'-I'+str(r/'include/nuttx'),str(p/'scheduler.c'),str(p/'driver-host.c'),'-o',out],check=True)
 subprocess.run([out],check=True)
print('PASS disabled stub link, mandatory enabled hook link failure, 11 config rejections, enabled split host UBSan. Not target OS build.')
