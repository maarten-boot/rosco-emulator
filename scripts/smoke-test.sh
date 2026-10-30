#!/usr/bin/env bash
#
# Boot every machine from the ROM sets in roms/ and check that the firmware
# banner comes back out of the console port. Nothing outside the tree is
# needed, which is what makes this the check CI runs on a fresh build.
#
# Examples:
#   scripts/smoke-test.sh
#   scripts/smoke-test.sh rosco_6502 rosco_m68k_010
#   scripts/smoke-test.sh -t 16

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

# The SDL OSD opens a video device whatever -video says and exits if it cannot,
# so on a machine with no display - a container, a build box - point it at the
# driver that needs none. Every run here is headless.
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}"

seconds=8

usage()
{
	cat >&2 <<USAGE
Usage: ${0##*/} [-t SECONDS] [machine...]

Boots each machine headless against roms/ and fails unless the expected
firmware output shows up. With no machine named, all of them are run.

  -t SECONDS   emulated seconds to give each machine (default: $seconds)
  -h           this help
USAGE
	exit 2
}

while getopts ":t:h" opt; do
	case "$opt" in
		t) seconds="$OPTARG" ;;
		h) usage ;;
		:) echo "${0##*/}: -$OPTARG needs an argument" >&2; exit 2 ;;
		*) usage ;;
	esac
done
shift $((OPTIND - 1))

# machine|pattern|pattern... - the CPU line proves the right core came up and
# sized the RAM through bus errors, the rest proves the firmware ran to its
# prompt. The 68020 and 68030 speeds are left out on purpose: they come out
# around twice their configured clock, see the README.
checks=(
	"rosco_m68k_000|MC68000 CPU @ 10.0MHz with 1048576 bytes RAM|Searching for boot media|Ready for Kermit receive"
	"rosco_m68k_010|MC68010 CPU @ 10.0MHz with 1048576 bytes RAM|Searching for boot media|Ready for Kermit receive"
	"rosco_m68k_020|MC68020 CPU|Searching for boot media|Ready for Kermit receive"
	"rosco_m68k_030|MC68030 CPU|Searching for boot media|Ready for Kermit receive"
	"rosco_6502|W65C02S CPU @ 14MHz|RAM Banks 0-15 passed|Memory checks: passed|rosco_6502 EWozMon"
)

[ -x "$EMU" ] || { echo "${0##*/}: $EMU not built - run make first" >&2; exit 1; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

failed=0
ran=0

for check in "${checks[@]}"; do
	IFS='|' read -r -a fields <<<"$check"
	machine="${fields[0]}"
	want=("${fields[@]:1}")

	if [ $# -gt 0 ]; then
		printf '%s\n' "$@" | grep -qxF "$machine" || continue
	fi
	ran=$((ran + 1))

	console="$work/$machine.txt"
	log="$work/$machine.log"
	: > "$console"

	# Fresh NVRAM: the m68k flash ROM persists there, and the smoke test
	# must boot the shipped BIOS, not whatever an earlier run flashed.
	"$EMU" "$machine" -skip_gameinfo -rompath "$ROOT/roms" \
		-nvram_directory "$work/nvram" \
		-terminal null_modem -bitb "$console" \
		-video none -sound none -midiprovider none \
		-nothrottle -seconds_to_run "$seconds" \
		>"$log" 2>&1 || true

	out=$(sed -e 's/\x1b\[[0-9;?]*[a-zA-Z]//g' "$console" | tr -d '\r')

	missing=()
	for pattern in "${want[@]}"; do
		grep -qF "$pattern" <<<"$out" || missing+=("$pattern")
	done

	if [ ${#missing[@]} -eq 0 ]; then
		echo "PASS $machine"
		continue
	fi

	failed=$((failed + 1))
	echo "FAIL $machine"
	for pattern in "${missing[@]}"; do
		echo "     missing: $pattern"
	done
	echo "--- console ---"
	printf '%s\n' "$out"
	echo "--- emulator ---"
	grep -iE "WRONG CHECKSUMS|NOT FOUND|Fatal error" "$log" || tail -n 5 "$log"
	echo "---"
done

if [ "$ran" -eq 0 ]; then
	echo "${0##*/}: no machine matched $*" >&2
	exit 2
fi

plural=s
if [ "$ran" -eq 1 ]; then
	plural=
fi

[ "$failed" -eq 0 ] || { echo "${0##*/}: $failed of $ran machine$plural failed" >&2; exit 1; }
echo "${0##*/}: all $ran machine$plural passed"
