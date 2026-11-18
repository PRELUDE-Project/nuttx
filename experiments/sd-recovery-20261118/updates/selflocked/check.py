from pathlib import Path
import subprocess,tempfile,json,argparse
parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);args=parser.parse_args()
p=Path(__file__).resolve().parent;r=args.repo.resolve();f=r/'tools/hosttests/scr_sector'
required=['CONFIG_SD_SECTOR_READ_REVIEW','CONFIG_ARCH_CHIP_STM32H7','CONFIG_STM32H7_SDMMC1','CONFIG_STM32H7_SDMMC_IDMA','CONFIG_SD_SCR_CONTAINMENT_REVIEW','CONFIG_SD_RECEIVE_LEASE_ONLY_REVIEW']
unsupported=['CONFIG_STM32H7_SDMMC2','CONFIG_SMP','CONFIG_ARCH_HIPRI_INTERRUPT','CONFIG_SCHED_TICKLESS','CONFIG_SCHED_INSTRUMENTATION','CONFIG_IRQCOUNT']
flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-pointer-to-int-cast','-Wno-sign-compare']
def defs(xs):return ['-D'+x+'=1' for x in xs]
with tempfile.TemporaryDirectory() as tmp:
 for case in ['nested','unlocked']:
  out=str(Path(tmp)/case)
  subprocess.run(['cc',*flags,*defs(required),'-DHOST_RECOVERY_TEST','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(p/'stubs'),'-I'+str(f),'-I'+str(f/'stubs'),'-I'+str(r/'include/nuttx'),str(p/'scheduler.c'),str(p/(case+'.c')),'-o',out],check=True)
  subprocess.run([out],check=True)
  print(case, 'PASS: duplicate notification, reentry, critical restored; host only')
