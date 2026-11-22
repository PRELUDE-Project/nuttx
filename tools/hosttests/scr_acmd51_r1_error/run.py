import hashlib
from pathlib import Path
import subprocess, sys, tempfile

if len(sys.argv)!=2:
    sys.exit("usage: python3 run.py /path/to/pinned-nuttx")
root=Path(sys.argv[1]).resolve()
here=Path(__file__).resolve().parent
fx=Path("tools/hosttests/scr_sector")
blobs={
    "arch/arm/src/stm32h7/stm32_sdmmc.c":"affa479b1cfebd404dd880d7ffa19466a07457ba",
    "drivers/mmcsd/mmcsd_sdio.c":"cc659e003afd1ec1fd4fee3d7b1a0b807bdc4270",
    "drivers/mmcsd/mmcsd_sdio.h":"76dcf15c6c4922c97f7e5f3b1180b304506c1274",
    "include/nuttx/scr_lease_review.h":"165fc50c187891041ce3aa8241bf27be72cbadc0",
    str(fx/"driver.c"):"443cb673fbcdf16b76d9e1617fa8e1b209796c7f",
    str(fx/"actual-command-constants.h"):"faf789f31bd9732ef802b1c429d949dd437996d9",
    str(fx/"stubs/nuttx/config.h"):"03e31f1e6dbafba84571cc95f2c301b3a7662193",
}
src={}
for p,want in blobs.items():
    data=(root/p).read_bytes()
    got=hashlib.sha1(f"blob {len(data)}\0".encode()+data).hexdigest()
    if got!=want:
        sys.exit(f"{p}: want {want}, got {got}")
    print(f"{got} {p}",flush=True)
    src[p]=data.decode().splitlines(keepends=True)
lower=src["arch/arm/src/stm32h7/stm32_sdmmc.c"]
upper=src["drivers/mmcsd/mmcsd_sdio.c"]
def code(lines, a, b):
    return "".join(lines[a-1:b])
base="".join(src[str(fx/"driver.c")]).split("\nint main(void)",1)[0]
if len(base.splitlines())>=len(src[str(fx/"driver.c")]):
    sys.exit("fixture boundary not found")
text=base+"\n"+code(lower,1947,1982)+"\n"+code(lower,3276,3333)+"\n"
defs="".join(l for l in src["drivers/mmcsd/mmcsd_sdio.h"] if l.split()[0:2] in [["#define",m] for m in ("MMCSD_R1_CARDISLOCKED","MMCSD_R1_ERROR","MMCSD_R1_ERRORMASK")])
if defs.count("#define")!=3: sys.exit("R1 constants not found")
text+=defs+"\n"
text+=(here/"fixture.c.in").read_text().replace("@UPPER@",code(upper,336,358)+code(upper,413,438)+code(upper,500,662))
with tempfile.TemporaryDirectory(prefix="scr-order-") as temp:
    out=Path(temp)
    (out/"proof.c").write_text(text)
    for opt in ("-O0","-O2"):
        print(opt,flush=True)
        subprocess.run(["gcc","-std=c11",opt,"-Wall","-Wextra","-Werror",
          "-Wno-unused-function","-Wno-unused-variable","-Wno-unused-parameter","-Wno-pointer-to-int-cast",
          "-fsanitize=undefined","-fno-sanitize-recover=all","-I"+str(root/fx),"-I"+str(root/fx/"stubs"),
          "-I"+str(root/"include/nuttx"),str(out/"proof.c"),"-o",str(out/"proof")],check=True)
        subprocess.run([str(out/"proof")],check=True)
