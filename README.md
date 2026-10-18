# rosco-emulator

An emulator for the [rosco_m68k](https://github.com/rosco-m68k/rosco_m68k) and
[rosco_6502](https://github.com/rosco-6502/rosco_6502) homebrew single board
computers, built as a slimmed-down MAME target.

This tree is MAME 0.289 with everything unrelated to rosco removed. The only
drivers that remain live in `src/rosco/drivers`, and the build produces a
single `rosco` executable instead of the full MAME binary.

The rosco_m68k driver originates from the SBC MAME fork at
[roscopeco/mame](https://github.com/roscopeco/mame), which is where these
machines were first emulated, and has been ported forward to current MAME here.
The rosco_6502 driver is new - see [Credits](#credits).

## Emulated systems

| Short name       | Description                    |
| ---------------- | ------------------------------ |
| `rosco_m68k_000` | rosco_m68k Classic V2, MC68000 |
| `rosco_m68k_010` | rosco_m68k Classic V2, MC68010 |
| `rosco_m68k_020` | rosco_m68k Classic V2, MC68020 |
| `rosco_m68k_030` | rosco_m68k Classic V2, MC68030 |
| `rosco_6502`     | rosco_6502 r4, W65C02S         |

The **Classic V2** machines emulate 1MB of DRAM, the XR68C681 DUART with both
serial channels, the bit-banged SPI SD card hanging off the DUART output port,
and the IDE/ATA interface. Unmapped addresses raise a bus error, which is what
the firmware uses to size memory and probe for expansion hardware.

The **rosco_6502** emulates the W65C02S at 14MHz, 16KB of low RAM, 16 banks of
32KB high RAM (512KB), 4 banks of 8KB ROM and the same XR68C681 DUART and SPI
SD card wiring as its bigger sibling. Banking goes through the register at
$0000, which the address decoder mirrors at $0001 and reads back from the RAM
underneath. The firmware's own power-on self test walks every RAM and ROM bank,
so a clean boot exercises the whole memory map.

### Known limitations

The firmware measures CPU speed against the DUART timer and prints the result
at boot. It comes out right for the 68000, 68010 and W65C02S, but the 68020 and
68030 read about twice their configured 20MHz: MAME's Musashi cores do not
model the prefetch and cache behaviour those parts have, so the calibration
loop runs through more iterations than it would on real silicon.

## Building

Requires a C++20 compiler, Python 3, SDL2 and the usual MAME build dependencies.
On Debian/Ubuntu:

```sh
sudo apt install build-essential git python3 pkg-config \
    libsdl2-dev libsdl2-ttf-dev libfontconfig-dev \
    libx11-dev libxinerama-dev libxext-dev libxi-dev libgl-dev \
    libasound2-dev libpulse-dev qt6-base-dev
```

| Package                          | Needed for                                        |
| -------------------------------- | ------------------------------------------------- |
| `build-essential`                | g++ and make                                      |
| `git`, `python3`, `pkg-config`   | the build scripts and dependency probing          |
| `libsdl2-dev`, `libsdl2-ttf-dev` | the SDL OSD: video, input, on-screen text         |
| `libfontconfig-dev`              | finding the UI font                               |
| `libx11-dev`, `libxinerama-dev`  | the X11 window and multi-monitor handling         |
| `libxext-dev`, `libxi-dev`       | XInput                                            |
| `libgl-dev`                      | the OpenGL renderer                               |
| `libasound2-dev`                 | ALSA, used by the MIDI module                     |
| `libpulse-dev`                   | the PulseAudio sound module                       |
| `qt6-base-dev`                   | the Qt debugger, see below                        |

Everything else MAME needs - zlib, expat, Lua, FLAC, SQLite, PortAudio, PortMidi
and the rest - is bundled in `3rdparty/` and built from source, so there are no
other packages to install. `libpipewire-0.3-dev` is picked up if it happens to be
installed and silently skipped otherwise.

```sh
make -j$(nproc)
```

`rosco` is the default target, so no `TARGET=` is needed. The resulting binary is
`./rosco` in the top of the tree.

Qt is only there for the debugger, and it is the one dependency worth dropping if
you do not want it - `qt6-base-dev` pulls in a lot for what it is. Building
without it gives you the SDL debugger instead, and `-debug` still works:

```sh
make -j$(nproc) REGENIE=1 USE_QTDEBUG=0
```

`REGENIE=1` is needed on the first build after changing any of these options: the
makefile does not notice that they changed, so without it the previously
generated project files are reused and the new setting is ignored.

### Prebuilt binaries

Every tagged release attaches `rosco-emulator-<version>-linux-x86_64.tar.gz`,
holding the stripped binary, the ROM sets, the test scripts and the bgfx
shaders. It is built on Ubuntu 22.04 without Qt, so it wants glibc 2.35 or
newer - Ubuntu 22.04, Debian 12 or anything since - and the runtime halves of
the packages above:

```sh
sudo apt install libsdl2-2.0-0 libsdl2-ttf-2.0-0 libfontconfig1 \
    libx11-6 libxinerama1 libxext6 libxi6 libgl1 libasound2 libpulse0
```

Anything older, or further afield than Debian, is what the container image is
for.

## Docker

```sh
docker pull solderdemon/rosco-emulator
docker run --rm --entrypoint rosco-smoke-test solderdemon/rosco-emulator
```

That second line boots all five machines and prints a line for each, which is
the same check CI runs on every build.

The image carries the emulator, the ROM sets and both test scripts. Its
entrypoint is the emulator itself, so anything after the image name goes
straight through to it, and the working directory is `/work`:

```sh
docker run --rm -v "$PWD:/work" solderdemon/rosco-emulator rosco_6502 \
    -video none -nothrottle -seconds_to_run 8 \
    -terminal null_modem -bitb console.txt
```

Headless is what the image is really for. Testing a firmware build from a
Makefile, or from someone else's CI, takes one line:

```sh
docker run --rm -v "$PWD:/work" --entrypoint rosco-test \
    solderdemon/rosco-emulator -e 'Memory checks: passed' boot32k.bin
```

The interactive terminal wants the X11 socket handed in, and `xhost +local:` on
the host if the connection is refused:

```sh
docker run --rm -it -e DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix \
    solderdemon/rosco-emulator rosco_m68k_010
```

The shipped ROM sets live in `/opt/rosco/roms`, which the `rompath` in
`~/.rosco/rosco.ini` points at; a `roms/` directory inside `/work` is searched
first, and `-rompath` on the command line overrides both. Sound and MIDI are
turned off in that same ini - these machines have neither, and leaving them on
means ALSA complaining about the `/dev/snd` that is not in the container on
every single run. The emulator
runs as uid 1000, so pass `--user "$(id -u):$(id -g)"` if what it writes into
`/work` should belong to someone else.

Building the image runs the same build as everywhere else, in a multi-stage
Dockerfile that keeps only the runtime libraries in the result:

```sh
docker build -t rosco-emulator .
```

Adding `--target export --output type=local,dest=.` builds the binary alone and
drops it in the current directory, if a container is not what you were after.

## Firmware

The rosco firmware is built from source by its users rather than being a fixed
ROM dump, so the ROM sets just need to contain whatever firmware you want to
run:

| ROM set                    | Contents                                       |
| -------------------------- | ---------------------------------------------- |
| `roms/rosco_m68k_<cpu>.zip`| `rosco_m68k.rom`                               |
| `roms/rosco_6502.zip`      | `rosco_6502.rom`, the four 8KB banks concatenated |

The hashes in the drivers describe the builds this tree was tested against. A
different firmware only gets a `WRONG CHECKSUMS` warning and still runs, so
there is no need to keep them in step while developing.

## Testing firmware and programs

`scripts/rosco-test.sh` boots a machine headless and prints whatever came out
of the console serial port. It can run a firmware image, a program, or both.

**A firmware image** is staged into a temporary ROM path, so `roms/` is left
alone and the checksums in the drivers do not matter:

```sh
scripts/rosco-test.sh boot32k.bin
scripts/rosco-test.sh -m rosco_m68k_010 -s sdcard.img rosco_m68k.rom
```

**A rosco_6502 program** normally boots through the firmware from a temporary
FAT32 SD card. `-p` creates the card with `/ROSC0DE_6502.BIN`, then the ROM
loads it at `$0800` and calls it. Returning with `RTS` goes back to WozMon.
The image is removed when the emulator exits, including interactive runs.

```sh
sudo apt install python3 dosfstools mtools
scripts/rosco-test.sh -p hello.bin -e 'hello, world'
scripts/rosco-test.sh -i -p hello.bin
scripts/rosco-test.sh -p hello.bin boot32k.bin
```

These dependencies are included in the Docker image. `-p` is for rosco_6502
only and cannot be combined with `-q` or `-s`. The image helper accepts
programs up to 512 KiB; quickload is limited to 14 KiB on the 6502. For an existing SD image, use
`-s sdcard.img` with a firmware image, or pass `-hard1 sdcard.img` directly
to the emulator. To keep a generated card:

```sh
python3 scripts/make-sdcard.py hello.bin sdcard.img
./rosco rosco_6502 -hard1 sdcard.img
```

**Quickload** goes straight into memory with `-q`, no Kermit and no SD card
needed. The firmware is left to finish booting first, then the program is
loaded and jumped to:

```sh
scripts/rosco-test.sh -q hello.bin                      # on the stock firmware
scripts/rosco-test.sh -q hello.bin boot32k.bin          # on your own firmware
```

Programs load at the address they are linked for: `$0800` on the rosco_6502 and
`0x40000` on the rosco_m68k. Note that a bare program says nothing about which
machine it is for, so pass `-m` unless the default guess is right.

**`-e` turns a run into a pass/fail check**, which is what makes this usable
from a Makefile or CI - it exits non-zero if the string never appears:

```sh
scripts/rosco-test.sh -e 'Memory checks: passed' boot32k.bin
scripts/rosco-test.sh -q hello.bin -e 'hello, world'
```

The firmware's own power-on self test walks every RAM and ROM bank, so the
first of those is a real smoke test of the whole memory map.

The SD regression test boots programs spanning multiple sectors and RAM banks,
checks their return to WozMon, and checks that quickload still runs:

```sh
python3 scripts/test-sd-boot.py
```

The bundled 6502 ROM comes from hardware repository commit
`d04e33043924147a309b692fe241f6d1910d6580`, with the sector-address fix in
`scripts/firmware-fat32.patch`. See [ROM provenance](roms/README.md) for
checksums and rebuild instructions.

**`scripts/smoke-test.sh` does the same for the tree itself**: it boots every
machine against the ROM sets in `roms/` and fails unless each one gets through
its banner and out to a prompt. It needs nothing but a built binary, takes
about eight seconds for all five, and is what CI runs on every push:

```sh
scripts/smoke-test.sh                    # all of them
scripts/smoke-test.sh -t 16 rosco_6502   # one, given longer
```

Other options: `-t SECONDS` for how long to run (default 8 emulated seconds,
about a second of wall clock), `-i` for a window instead of headless, `-r` to
keep the ANSI escapes, and `--` to pass anything else through to the emulator.

The emulator takes `-quik` directly too, if you would rather not go through the
script:

```sh
./rosco rosco_6502 -quik hello.bin
```

## Running

```sh
# interactive, with the built-in terminal
./rosco rosco_m68k_010

# headless, with the console redirected to a file
./rosco rosco_m68k_010 -terminal null_modem -bitb console.txt -video none

# attach a pseudo-terminal instead, and talk to it with screen/minicom
./rosco rosco_m68k_010 -terminal pty
```

Both serial ports default to 115200 8N1, matching the firmware.

An SD card image goes on the first hard disk slot and the IDE disk on the second:

```sh
./rosco rosco_m68k_010 -hard1 sdcard.img -hard2 ide.chd
./rosco rosco_6502 -hard1 sdcard.img
```

Raw images work as-is; `.chd` images can be created with `chdman`, built by
`make TOOLS=1`. The rosco_6502 has no IDE interface, so it only takes the SD
card.

The rosco_m68k machines have 1MB of RAM by default. `-ram` adds the RAM
expansion above it, contiguous as on the real board (total 2M to 14M; `-ram
5m` is the 4MB expansion board). `-bios v242` runs firmware 2.42 instead of
the default 2.50.DEV build (`-listbios` lists them):

```sh
./rosco rosco_m68k_010 -bios v242 -ram 5m -hard1 sdcard.img
```

Firmware 2.42 stores its kernel API pointer at 0x400, over the System Data
Block magic; programs that check the magic before reading the SDB (such as
the memory size at 0x414) see no SDB under 2.42.

## Credits

The rosco_m68k drivers come from the SBC MAME fork,
[roscopeco/mame](https://github.com/roscopeco/mame), by Ross Bamford and
contributors, with portions by Chris Hanson. Porting them to MAME 0.289
required updating the bus error handling for the rewritten 68000 core and the
usual API churn; the hardware emulation itself is theirs.

The rosco_6502 driver was written for this tree from the board's schematic,
PLD equations and firmware sources, which document the memory map and the DUART
pin assignments in full.

The rosco computers themselves are by The Really Old-School Company:
[rosco_m68k](https://github.com/rosco-m68k/rosco_m68k) and
[rosco_6502](https://github.com/rosco-6502/rosco_6502).

## License

MAME is distributed under the terms in `COPYING` (GPL-2.0-or-later). The rosco
drivers carry their own MIT / BSD-3-Clause notices in their source headers.
