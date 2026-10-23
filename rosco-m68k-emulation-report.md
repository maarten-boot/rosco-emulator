# rosco_m68k emulation — repository report

Date: 2026-10-23 · Written against `rosco-m68k-ram-bios` at `f283c471eda` and
updated after the fixes merged in PR #2 (`b1ce74f4950`).

## 1. What this repository is

A slimmed-down **MAME 0.289** tree that builds a single `rosco` executable
instead of the full `mame` binary. Everything unrelated to the rosco boards has
been stripped; the only drivers left are in `src/rosco/drivers/`:

| File | Lines | Machines |
| --- | --- | --- |
| `src/rosco/drivers/rosco_m68k.cpp` | 631 | `rosco_m68k_000`, `_010`, `_020`, `_030` |
| `src/rosco/drivers/rosco_6502.cpp` | 288 | `rosco_6502` (not covered here) |

`src/rosco/rosco.cpp` holds the app-name constants and `src/rosco/rosco.lst`
the driver list. The m68k driver was ported from the SBC fork
`roscopeco/mame`; the port mainly involved adapting bus-error signalling to the
rewritten MAME 68000 core.

Supporting pieces:

- `roms/rosco_m68k_0{00,10,20,30}.zip`: identical ROM sets with two BIOSes
  (see §3). Provenance is in `roms/README.md`.
- `scripts/smoke-test.sh`: boots every machine headless and checks the banner
  (this is what CI runs). `scripts/rosco-test.sh` runs a freshly built firmware
  or quickloads a program, with `-e PATTERN` for pass/fail. Other scripts
  (`make-sdcard.py`, `test-sd-boot.py`, `update-6502-rom.py`,
  `firmware-fat32.patch`) are mostly for the 6502.
- `.github/workflows/ci.yml` (build + smoke test, no Qt) and `release.yml`
  (tagged tarball), plus a `Dockerfile`.
- Untracked local state (all gitignored): the `rosco` binary (89 MB, built
  2026-10-18 23:13), `build/`, `cfg/`, `snap/`, `error.log` (557 KB),
  `debug.log`.

### Recent history on this branch

| Commit | Change |
| --- | --- |
| `de3ef70395b` | `-ram` expansion (1M–14M) and `-bios v242` for rosco_m68k |
| `93e0ae532cc` | gdb stub: map `m68010` to the 68000 register set |
| `9d9c8ed08b4` | `-debugger none` now runs `-debugscript` before resuming |
| `0c155ca9cca` | real bus-error fault address; quickload into expansion RAM (§5 items 1–2) |
| `3b49f867ab6` | ignore `tmp/` |

`93e0ae532cc` and `9d9c8ed08b4` were rewritten (and force-pushed) on
2026-10-23 to replace a placeholder author identity; they were originally
`e06207686e2` and `f283c471eda`. The two debugger commits are the only changes this tree makes to MAME's core
(`src/emu`, `src/devices`, `src/osd`) since the rosco target was created.

## 2. Emulated hardware (rosco_m68k Classic V2)

### CPU variants

| Machine | CPU class | Clock | Data bus / ROM region | Bus-error path |
| --- | --- | --- | --- | --- |
| `rosco_m68k_000` | `M68000` (new MAME core) | 10 MHz | 16-bit, `ROM_REGION16_BE` | `m68000_device::trigger_bus_error()` |
| `rosco_m68k_010` | `M68010` (Musashi) | 10 MHz | 16-bit | `set_buserror_details()` + pulse `M68K_LINE_BUSERROR` |
| `rosco_m68k_020` | `M68020` (Musashi) | 20 MHz | 32-bit, `ROM_REGION32_BE` | same as 010 |
| `rosco_m68k_030` | `M68030` (Musashi) | 20 MHz | 32-bit | same as 010 |

The class hierarchy is `rosco_m68k_state` → (`rosco_m68k_musashi_state`) →
per-CPU state. Each of the four leaf classes has its own copy of
`delegated_mem_map`, `delegated_cpu_space_map`, `bootvec_reset` and
`bootvect_w`. Apart from data width and the 32-bit extra map, these copies are
identical.

### Memory map (`mem_map`, line 363)

| Range | Device |
| --- | --- |
| `000000–000007` | boot-vector view: reads ROM; the first write goes to RAM and disables the view |
| `000000–0FFFFF` | 1 MB on-board RAM (`sysram` share) |
| `100000–DFFFFF` | **bus error**; replaced by RAM up to `-ram` size in `machine_start()` |
| `E00000–EFFFFF` | ROM `monitor` (1 MB max) |
| `F00000–F0001F` | XR68C681 DUART, odd bytes (`umask16(0x00ff)`) |
| `F00020–F8003F` | bus error (expansion) |
| `F80040–F8004F` / `F80050–F8005F` | ATA CS0 / CS1, IRQ 3 |
| `F80060–FFFDFF` | bus error (expansion) |
| `FFFE00–FFFFFF` | **unmapped, no bus error** (MAME default open bus) |
| `01000000–FFFFFFFF` (020) / `–FFFFFDFF` (030) | bus error |

CPU space: autovectors at `…FFFFF0`, with the DUART supplying a vectored
interrupt at `…FFFFF9` (level 4).

### Peripherals

- **DUART** XR68C681 on its own 3.6864 MHz crystal, IRQ 4. Channel A goes to
  the `terminal` RS-232 port (default `terminal` device) and channel B to
  `host` (empty by default). Both are 115200 8N1.
  - IP0/IP1 = CTS A/B. IP2 = SD MISO.
  - OP0/OP1 = RTS A/B. OP2 = SD CS (inverted). OP4 = SCK. OP6 = MOSI. OP3/OP5
    = red/green LED, whose callbacks are empty stubs.
  - OP7 (`SPI_CS1`, a second SPI device) is listed in the comment but not
    wired.
- **SD card**: `spi_sdcard_device`, bit-banged through the DUART, attached as
  `-hard1`.
- **IDE**: `ata_interface_device`, one `hdd` slot, attached as `-hard2`.
- **RAM**: `ram_device` with sizes 1M (default), 2–6M, 8M, 10M, 12M and 14M. The
  first 1 MB of the RAM device buffer is allocated but unused, because the
  on-board RAM is the separate `sysram` share.
- **Quickload** (`-quik file.bin|.rom`): after a 3 s delay it copies the file to
  `0x40000` and sets PC there.
- No sound and no video hardware (`MACHINE_NO_SOUND_HW`); the console is the
  serial terminal.

### Reset / boot-vector handling

At reset, `machine_reset()` selects view 0, so `0–7` reads the initial SSP/PC
from the ROM at `E00000`. The firmware's first write there lands in `sysram`
and calls `m_bootvect.disable()`. This matches the hardware description in the
file header.

### Bus error emulation

`set_bus_error()` (line 530) ignores side-effect-free (debugger) accesses. It
signals the CPU and then masks further bus errors for 16 CPU cycles, so that a
read-modify-write cycle does not fault twice. The firmware relies on this to
size RAM and probe for expansion hardware, and the `-ram` banner results below
show it working.

## 3. Firmware

| BIOS | File | Size | CRC32 |
| --- | --- | --- | --- |
| `v250` (default) | `rosco_m68k.rom`, 2.50.DEV | 0xDA000 | `13eb1ac1` |
| `v242` | `rosco_m68k_242.rom`, built unmodified from `release/version-2.42` | 0xAB10 | `86950afb` |

The build recipe for 2.42 is documented in `roms/README.md`. The driver's
comment block (line 562) explains how to run a self-built ROM
(`-novalidate` / `scripts/rosco-test.sh`).

## 4. Verification run (this session, existing binary)

| Test | Result |
| --- | --- |
| `scripts/smoke-test.sh rosco_m68k_000 … _030` | **4/4 PASS** |
| `rosco_m68k_010 -bios v250 -ram {1m,5m,14m}` | banner reports 1048576 / 5242880 / 14680064 bytes RAM ✔ |
| `rosco_m68k_010 -bios v242 -ram {1m,5m,14m}` | same values ✔ |
| CPU speed banner | 000: 10.0 MHz, 010: 10.0 MHz, **020: 42.8 MHz, 030: 45.7 MHz** |

The 020/030 speed readings match the limitation the README documents: the
Musashi cores do not model prefetch or cache timing, so the firmware's
calibration loop reads about twice the configured 20 MHz.

These runs used the binary from 2026-10-18. The binary has since been rebuilt
with the fixes below, and all five machines (including `rosco_6502`) still
pass `scripts/smoke-test.sh`.

### Bus-error fix verification

A hand-assembled test program was quickloaded at `0x40000`. It points the
bus-error vector at a handler that copies the exception frame to `0x3000 +
0x20*n` and resumes with the next test. A `-debugger none -debugscript` run
(`gtime #6000`, then `save`) dumped the frames. Fault address reported:

| Access | Before (68010) | After (68000, 68010, 68020, 68030) |
| --- | --- | --- |
| `move.b $200001,d0` | `0x100000` | `0x200001` |
| `move.w d0,$200000` | `0x100000` | `0x200000` |
| `move.b $F00021,d0` | `0xF00000` | `0xF00021` |
| `move.w $F80060,d0` | `0xF00000` | `0xF80060` |
| `move.w $2000000,d0` (68020/68030 only) | — | `0x2000000` |

The read/write bit in the special status word was already correct before the
fix, because Musashi takes it from its own access state for these frames. A
900 KB quickload image is rejected with `-ram 1m` and loads and runs with
`-ram 2m`.

## 5. Findings and observations

Ordered roughly by impact.

1. **[FIXED in `0c155ca9cca`] Quickload ignored RAM expansion.** `quickload_cb`
   hard-coded `RAM_END = 0x100000`, so programs larger than 768 KB were
   rejected even with `-ram 14m`. It now uses `m_ram->size()` (line 333).

2. **[FIXED in `0c155ca9cca`] Bus-error fault addresses were wrong on the
   Musashi CPUs.** In MAME, a handler's `offset` is relative to the start of
   its mapped range, but the handlers rebuilt the address as if it were not:
   - `unmapped_ram_r/w` reports `offset<<1`, which is off by `0x100000`. For
     the 020/030 upper range it is off by `0x01000000`.
   - `unmapped_exp_r/w` adds `0xf00000`, but the two ranges start at
     `0xf00020` and `0xf80060`.

   Only the 010/020/030 use this address (it goes into
   `set_buserror_details()` and so into the exception stack frame and the 020+
   fault address). The 68000 core tracks the address itself. Sizing and
   probing still work, but a bus-error handler that prints or decodes the
   fault address saw the wrong value. The handlers are now
   `unmapped_r/w<Base>`, templated on the range start, and add 1 for a
   low-byte (odd address) access. The read flag passed to
   `set_buserror_details()` was also inverted (its API says 1 = read) and was
   corrected at the same time. See §4 for the verification.

3. **`FFFE00–FFFFFF` is a hole with no bus error.** On the 000/010, and for the
   mirrored low 16 MB on the 020/030, reads from this range return 0 silently.
   The 030 extra map also stops at `FFFFFDFF`, while the 020 map goes to
   `FFFFFFFF`. This may be deliberate (it leaves room for something at the top
   of memory), but there is no comment saying so. Check it against the board's
   decode logic.

4. **DUART log spam.** The firmware writes `OPCR = 0x08` (OP3 = TxCB 1× clock
   in the 68681 encoding). `mc68681.cpp:1015` only handles the `0x04` timer
   output, so it logs `Unhandled OPCR value: 08`, which accounts for 9,926 lines
   of the local `error.log`. It is harmless, because OP3 drives only the
   unimplemented red LED. Possible fixes: silence the message, or tell the
   firmware authors, since the heartbeat LED was probably meant to be `0x04`.

5. **LEDs are not exported.** `write_red_led` and `write_green_led` are
   empty. Calling `output_finder<2>` would let them show in the UI or Lua and
   could be used in tests.

6. **Duplicated per-CPU code.** About 130 lines (the four copies of
   `delegated_mem_map`, `bootvec_reset`, `bootvect_w` and the cpu-space maps)
   could become a template on the data width (`uint16_t` / `uint32_t`). The
   view setup and cpu-space map could also move into the base class.

7. **Minor driver nits.**
   - A comment at line 485 says the boot vector comes from `0x008000`, but the
     code uses ROM offset 0 at `0xE00000`.
   - The DUART comment lists IP3–IP5 as `???`.
   - The OP7 `SPI_CS1` comment has no matching code.

8. **Repo hygiene.**
   - **[FIXED]** Commit `e06207686e2` had a placeholder author identity and
     the message "debugstub". It was rewritten as `93e0ae532cc` and
     force-pushed to `master` and `rosco-m68k-ram-bios`.
   - `cfg/rosco_m68k_010.cfg` contains an absolute NFS path to a MINIX image
     directory. It is ignored by git, so this is only local state.

## 6. Coverage summary

| Area | Status |
| --- | --- |
| 68000/010/020/030 CPUs | ✔ (020/030 timing approximate) |
| 1 MB RAM + expansion to 14 MB | ✔ |
| ROM, two BIOS choices | ✔ |
| Boot-vector overlay | ✔ |
| Bus error on unmapped access | ✔ (fault address fixed 2026-10-23) |
| DUART, two serial channels, timer IRQ | ✔ |
| SPI SD card (CS0) | ✔ |
| Second SPI device (OP7) | ✘ |
| IDE/ATA | ✔ |
| LEDs | ✘ (stubs) |
| Quickload | ✔ (uses all RAM since 2026-10-23 fix) |
| Expansion bus devices (video, sound, Ethernet, etc.) | ✘ (bus error only) |
| CI smoke test | ✔ (all four m68k machines pass) |
