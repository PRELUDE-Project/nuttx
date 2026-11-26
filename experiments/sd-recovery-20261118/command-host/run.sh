#!/bin/sh
set -eu
here=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
source=${1:?Usage: sh run.sh /path/to/scrdiag_main.c}
case "$source" in /*) ;; *) source=$PWD/$source ;; esac
[ "$(git hash-object "$source")" = 18c8ad3cb0ca92d95d230db52f268d4dbf078209 ]
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT HUP INT TERM
mkdir "$tmp/nuttx"
: > "$tmp/nuttx/config.h"
printf '%s\n' 'typedef unsigned irqstate_t;' 'irqstate_t enter_critical_section(void);' 'void leave_critical_section(irqstate_t);' > "$tmp/nuttx/irq.h"
printf '%s\n' 'struct sdio_dev_s;' > "$tmp/nuttx/sdio.h"
printf '%s\n' '#include <stddef.h>' '#include <stdint.h>' 'int mmcsd_scr_diagnostic(struct sdio_dev_s *);' 'int mmcsd_sector_readonce(struct sdio_dev_s *, uint32_t, uint8_t *, size_t);' > "$tmp/nuttx/mmcsd_scrdiag.h"
for opt in -O0 -O2; do
  for kind in with without; do
    set --
    if [ "$kind" = with ]; then set -- -DCONFIG_SD_SECTOR_READ_REVIEW; fi
    "${CC:-cc}" "$opt" "$@" -I "$tmp" -std=c11 -Wall -Werror -UNDEBUG \
      -fsanitize=undefined -fno-sanitize-recover=all "$source" "$here/fixture.c" -o "$tmp/test"
    for mode in sector scr fail; do
      printf '%s %s %s\n' "$opt" "$kind" "$mode"
      "$tmp/test" "$mode"
    done
  done
done
