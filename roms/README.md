# rosco_6502 ROM

The bundled `rosco_6502.zip` contains four concatenated 8 KiB banks built from
the rosco_6502 hardware repository at commit
`d04e33043924147a309b692fe241f6d1910d6580` (System 0.03.DEV), using cc65 2.19.
The source firmware files were unmodified at that revision before applying
the patch below. The console configuration in `bank0.s` is 115200 8N1.

Local patch: [`firmware-fat32.patch`](../scripts/firmware-fat32.patch).
In `_FAT_READFILE`, `CMP #0` sets carry after a successful sector read.
The subsequent `ADC #2` therefore advances the destination by three pages,
leaving gaps between sectors. The patch clears carry before advancing by
two pages (512 bytes). It does not change the hardware repository.

Patched ROM checksums:

* CRC32: `4ed0320a`
* SHA1: `81c9437101175a64912d2863ad48ee4ac002d80e`

To reproduce from a checkout of that commit, with Python 3, cc65 and `patch`
on PATH (Linux or WSL):

```sh
python3 scripts/update-6502-rom.py /path/to/rosco_6502
make -j4 USE_QTDEBUG=0 SYMBOLS=0 STRIP_SYMBOLS=1
scripts/smoke-test.sh
python3 scripts/test-sd-boot.py
```

The update script builds in a temporary copy, applies the patch, and updates
the archive and the driver's checksums. The tests additionally require
`dosfstools` and `mtools`. When moving to a newer upstream firmware revision,
review whether the patch is still needed and update this provenance record.

The firmware is copyright Ross Bamford and the rosco_6502 contributors,
distributed under the MIT license in its source repository.

The patched firmware was also rebuilt successfully with the Windows rosco CLI
0.3.0 and its existing `solderdemon/rosco_6502:latest` Docker toolchain. It
produced the same ROM SHA1. For a copied, patched firmware directory, place
this `rosco.toml` alongside its Makefile:

```toml
[project]
board = "rosco_6502"
type = "firmware"
artifact = "boot32k.bin"

[build]
toolchain = "docker"
program = "make"
args = ["boot32k.bin"]
clean_args = ["clean"]

[build.docker]
image = "solderdemon/rosco_6502:latest"
```

Then run `rosco -C /path/to/patched-firmware build --docker --clean`.
This builds firmware; the emulator executable uses the C++/MAME Makefile.

# rosco_m68k firmware 2.42

`rosco_m68k_000.zip` to `rosco_m68k_030.zip` also contain
`rosco_m68k_242.rom`, selected with `-bios v242` (the default BIOS `v250` is
the 2.50.DEV `rosco_m68k.rom`). It is built, unmodified, from the rosco_m68k
repository at `release/version-2.42`:

```sh
git archive release/version-2.42 code | tar -x -C /tmp/fw242
cd /tmp/fw242/code/firmware/rosco_m68k_firmware
docker run --rm -u "$(id -u):$(id -g)" --group-add 1001 -v /tmp/fw242:/tmp/fw242 \
    -w "$PWD" --entrypoint /usr/bin/env roscopeco/rosco_m68k \
    PATH=/home/linuxbrew/.linuxbrew/bin:/usr/bin:/bin make rosco_m68k.rom
```

(m68k-elf-gcc 13.3 and vasm from the `roscopeco/rosco_m68k` image; the
default 68000 build, 1MB HUGEROM SST39SF040, console at 115200.)

* Size: 43792 bytes (0xab10)
* CRC32: `86950afb`
* SHA1: `6ff2a6584b2f504e73428ffae2761deecbcffb5b`
