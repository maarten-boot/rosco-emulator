#!/usr/bin/env bash
#
# Runs a machine headless for 6 emulated seconds, then dumps 24 bytes of
# memory through the debugger and prints them with the banner count.
#
#   dbgrun.sh MACHINE NVRAM_DIR ADDR [PROGRAM [extra emulator args...]]
#
# ADDR is hex without a prefix. PROGRAM is quickloaded at 0x40000. Needs a
# rosco binary built with the -debugger none script fix (9d9c8ed08b4).

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
machine=$1 nvram=$2 addr=$3 prog=${4:-}
shift $(( $# < 4 ? $# : 4 ))

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
printf 'gtime #6000\nsave %s/dump.bin,%s,18\ng\n' "$work" "$addr" > "$work/script.txt"

SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}" "$ROOT/rosco" "$machine" -rompath "$ROOT/roms" \
	-nvram_directory "$nvram" ${prog:+-quik "$prog"} \
	-debug -debugger none -debugscript "$work/script.txt" \
	-terminal null_modem -bitb "$work/console.txt" \
	-video none -sound none -nothrottle -seconds_to_run 8 -skip_gameinfo "$@" \
	> "$work/emu.log" 2>&1 || true

dump=$( [ -f "$work/dump.bin" ] && xxd -p -l 24 "$work/dump.bin" || echo "(none: CPU halted?)" )
printf 'mem[%s] = %s | banner: %s\n' "$addr" "$dump" "$(grep -a -c 'CPU @' "$work/console.txt" || true)"
