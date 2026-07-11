#!/bin/zsh
# Interactive diagnostic run of the port build.
#
#   ./run_diag.sh          -- instrumented run, stderr captured to loco/run_diag.log
#   ./run_diag.sh heap     -- same, plus wine heap validation on every alloc/free
#                             (SLOW -- the RFH parse alone is O(n^2) over ~4000
#                             entries -- but it reports the corrupted block at the
#                             first operation after the bad write, not minutes later)
#
# LOCO_PORT_CLICK / LOCO_PORT_DUMP / LOCO_PORT_STAT are forwarded from the caller's
# environment, so an unattended run can drive its own input:
#
#   LOCO_PORT_CLICK="60:skip;240:alone;360:enter" LOCO_PORT_DUMP=60 ./run_diag.sh
#
# See port/PortMode.h for the script grammar and the registered rect names.
#
# Click through to the game as usual. Afterwards: loco/run_diag.log has the wine
# stderr + any backtrace, loco/port_trace.log has the Port_Tracef diagnostics, and
# loco/stub_calls.log has the stub hits.
set -e
cd "$(dirname "$0")"
pkill -f Loco-port.exe 2>/dev/null || true
pkill -f wineserver 2>/dev/null || true
sleep 1

cp build/Loco-port.exe loco/Loco-port.exe
[[ -f build/Loco-port.ilk ]] && cp build/Loco-port.ilk loco/Loco-port.ilk

rm -f loco/port_trace.log loco/stub_calls.log loco/run_diag.log

if [[ "$1" == "heap" ]]; then
  export WINEDEBUG=warn+heap
fi

cd loco
PATH="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
DYLD_LIBRARY_PATH="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib64:/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib32on64" \
LOCO_PORT_STAT=60 \
/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine --bottle "General" \
  "$(pwd)/Loco-port.exe" 2>&1 | tee run_diag.log
