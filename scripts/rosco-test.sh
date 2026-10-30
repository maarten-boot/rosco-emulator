#!/usr/bin/env bash
#
# Run a freshly built rosco firmware image in the emulator.
#
# The firmware is staged into a temporary ROM path, so the checksums baked into
# the drivers do not matter and nothing in roms/ is touched. MAME warns about
# the mismatch and runs anyway.
#
# Examples:
#   scripts/rosco-test.sh boot32k.bin
#   scripts/rosco-test.sh -m rosco_m68k_010 rosco_m68k.rom
#   scripts/rosco-test.sh -e 'Memory checks: passed' boot32k.bin
#   scripts/rosco-test.sh -i -s sdcard.img boot32k.bin
#   scripts/rosco-test.sh -q hello.bin            # run a program on stock firmware

set -euo pipefail

# Followed through any symlinks first: the container puts these scripts on the
# PATH as /usr/local/bin/rosco-smoke-test and friends, pointing at the real
# copies under /opt/rosco, and the tree they belong to is the one at the far
# end of the link.
self="${BASH_SOURCE[0]}"
while [ -L "$self" ]; do
	link="$(readlink "$self")"
	case "$link" in
		/*) self="$link" ;;
		*)  self="$(dirname "$self")/$link" ;;
	esac
done

ROOT="$(cd "$(dirname "$self")/.." && pwd)"
EMU="$ROOT/rosco"

machine=""
program=""
sdprogram=""
seconds=8
sdcard=""
interactive=0
expect=""
raw=0

usage()
{
	cat >&2 <<EOF
Usage: ${0##*/} [options] [firmware]

Runs <firmware> as the machine's ROM. With -p or -q, <firmware> is optional.

  -m MACHINE   system to run (default: guessed from the image size)
               rosco_6502, rosco_m68k_000/_010/_020/_030
  -p PROGRAM   boot a rosco_6502 program through firmware and a temporary
               FAT32 SD card (requires python3, dosfstools and mtools)
  -q PROGRAM   load PROGRAM into RAM once the firmware is up and run it
               (rosco_6502: \$0800, rosco_m68k: 0x40000)
  -t SECONDS   emulated seconds to run headless (default: $seconds)
  -s IMAGE     attach IMAGE as the SPI SD card
  -i           run interactively in a window instead of headless
  -e PATTERN   fail unless PATTERN appears in the console output
  -r           keep ANSI escapes in the output
  -h           this help

Anything after -- is passed straight through to the emulator.
EOF
	exit 2
}

while getopts ":m:p:q:t:s:e:irh" opt; do
	case "$opt" in
		m) machine="$OPTARG" ;;
		p) sdprogram="$OPTARG" ;;
		q) program="$OPTARG" ;;
		t) seconds="$OPTARG" ;;
		s) sdcard="$OPTARG" ;;
		e) expect="$OPTARG" ;;
		i) interactive=1 ;;
		r) raw=1 ;;
		h) usage ;;
		:) echo "${0##*/}: -$OPTARG needs an argument" >&2; exit 2 ;;
		*) usage ;;
	esac
done
shift $((OPTIND - 1))

firmware=""
if [ $# -ge 1 ] && [[ "$1" != -* ]]; then
	firmware="$1"; shift
elif [ -z "$program" ] && [ -z "$sdprogram" ]; then
	usage
fi

if [ -n "$sdprogram" ]; then
	[ -z "$program" ] && [ -z "$sdcard" ] ||
		{ echo "${0##*/}: -p cannot be combined with -q or -s" >&2; exit 2; }
	machine="${machine:-rosco_6502}"
	[ "$machine" = rosco_6502 ] ||
		{ echo "${0##*/}: -p supports rosco_6502 only" >&2; exit 2; }
fi

[ -x "$EMU" ] || { echo "${0##*/}: $EMU not built - run make first" >&2; exit 1; }
[ -n "$firmware" ] && [ ! -f "$firmware" ] &&
	{ echo "${0##*/}: no such file: $firmware" >&2; exit 1; }
[ -n "$program" ] && [ ! -f "$program" ] &&
	{ echo "${0##*/}: no such file: $program" >&2; exit 1; }

# Guess the machine from whichever image we were given: the 6502 ROM is one or
# four 8KB banks and its programs live in 14KB of low RAM, so anything bigger
# has to be m68k.
if [ -z "$machine" ]; then
	size=$(stat -c%s "${firmware:-$program}")
	if [ "$size" -le 32768 ]; then
		machine="rosco_6502"
	else
		machine="rosco_m68k_010"
	fi
	if [ -z "$firmware" ]; then
		# A bare program says nothing about the architecture - both machines
		# run small binaries - so this guess is worth flagging harder.
		echo "${0##*/}: no firmware given, guessing $machine from a ${size} byte program;" >&2
		echo "${0##*/}: pass -m if that is the wrong machine" >&2
	else
		echo "${0##*/}: assuming $machine for a ${size} byte image (override with -m)" >&2
	fi
fi

case "$machine" in
	rosco_6502)      romname="rosco_6502.rom" ;;
	rosco_m68k_*)    romname="rosco_m68k.rom" ;;
	*) echo "${0##*/}: unknown machine: $machine" >&2; exit 2 ;;
esac

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# Fresh NVRAM each run, so the flash ROM is seeded from the firmware under
# test instead of keeping what an earlier run (or updateflash) left there.
args=( "$machine" -skip_gameinfo -nvram_directory "$work/nvram" )
if [ -n "$sdprogram" ]; then
	sdcard="$work/sdcard.img"
	python3 "$ROOT/scripts/make-sdcard.py" "$sdprogram" "$sdcard"
fi
if [ -n "$firmware" ]; then
	mkdir -p "$work/roms/$machine"
	cp "$firmware" "$work/roms/$machine/$romname"
	args+=( -rompath "$work/roms" )
fi
[ -n "$program" ] && args+=( -quik "$program" )
[ -n "$sdcard" ] && args+=( -hard1 "$sdcard" )

if [ "$interactive" -eq 1 ]; then
	"$EMU" "${args[@]}" "$@"
	exit $?
fi

# The SDL OSD opens a video device whatever -video says and exits if it cannot,
# so a headless run on a machine with no display - a container, a build box -
# needs the driver that needs none.
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}"

# Headless: the console comes out of the DUART's channel A into a bitbanger.
console="$work/console.txt"
: > "$console"
args+=( -terminal null_modem -bitb "$console"
        -video none -sound none -midiprovider none -nothrottle -seconds_to_run "$seconds" )

status=0
"$EMU" "${args[@]}" "$@" >"$work/emu.log" 2>&1 || status=$?
if [ "$status" -ne 0 ]; then
	cat "$work/emu.log" >&2
	exit "$status"
fi
grep -iE "WRONG CHECKSUMS|NOT FOUND|Fatal error" "$work/emu.log" >&2 || true

if [ "$raw" -eq 1 ]; then
	cat "$console"
else
	sed -e 's/\x1b\[[0-9;?]*[a-zA-Z]//g' "$console" | tr -d '\r'
fi

if [ -n "$expect" ]; then
	if sed -e 's/\x1b\[[0-9;?]*[a-zA-Z]//g' "$console" | tr -d '\r' | grep -qF "$expect"; then
		echo "${0##*/}: PASS - found '$expect'" >&2
	else
		echo "${0##*/}: FAIL - '$expect' not in console output" >&2
		exit 1
	fi
fi
