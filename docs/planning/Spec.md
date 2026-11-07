# Spec — rosco_m68k emulation

What the `rosco_m68k_*` machines in this tree must do. Each item has an ID
that `Tests.md` and `Plan.md` refer to. Status is as of 2026-10-30.

All items are on `master` (`038df42e8be`; S10/S11 came in with PR #6).

**Crosscheck 2026-10-30** against `master` `038df42e8be`: every item
below was found at the place named in "Where", and nothing in the code
contradicts this spec. The tests in `Tests.md` were re-run on a fresh
build of that commit; only T16 is still open.

| ID | Requirement | Status | Where |
| --- | --- | --- | --- |
| S1 | Four machines: `rosco_m68k_000` (MC68000) and `_010` (MC68010) at 10 MHz, and `_020` (MC68020) and `_030` (MC68030) at 20 MHz. | done | `rosco_m68k.cpp` machine configs |
| S2 | Memory map: 1 MB RAM at `000000`, 1 MB ROM at `E00000`, XR68C681 DUART at `F00000` (odd bytes), ATA at `F80040`/`F80050`. | done | `mem_map` |
| S3 | At reset, `000000–000007` reads the first 8 bytes of ROM (initial SSP/PC). The first write there goes to RAM and removes the overlay until the next reset. | done | `m_bootvect` view |
| S4 | Accesses to unmapped RAM and expansion space raise a bus error. | done | `unmapped_r/w<Base>` |
| S4.1 | The bus-error stack frame holds the real fault address (odd address for a low-byte access) on all four CPUs. | done (`0c155ca9cca`) | `set_bus_error` |
| S4.2 | One fault per access: further bus errors are masked for 16 CPU cycles, so a read-modify-write cycle doesn't fault twice. | done | `m_bus_error_timer` |
| S5 | `-ram` adds expansion RAM directly above the 1 MB, for a total of 1M–14M, and the firmware reports the total. | done (`de3ef70395b`) | `machine_start` |
| S6 | `-bios v250` (default, 2.50.DEV) and `-bios v242` (2.42) select the firmware. | done (`de3ef70395b`) | `ROM_START` blocks |
| S7 | Quickload (`-quik`, `.bin`/`.rom`) copies a program to `0x40000` 3 s after start and jumps to it. The program may run up to the end of the configured RAM. | done (`0c155ca9cca`) | `quickload_cb` |
| S8 | DUART: channel A is the console (115200 8N1), channel B is the host port. The SPI SD card is bit-banged on OP2/OP4/OP6/IP2. IRQ 4 is vectored by the DUART. | done | `rosco_m68k()` config |
| S9 | The console can be reached over TCP with `-terminal null_modem -bitb socket.<host>:<port>`, input and output both ways. | done (MAME feature) | — |
| S10 | The ROM is two SST39SF040 flash chips: even bytes (D15–D8) and odd bytes (D7–D0). | done (PR #6) | `flash_r/w` |
| S10.1 | Software ID returns maker `0xBF` and device `0xB7` per chip. | done | `intelfsh` |
| S10.2 | Sector erase (4 KB) and chip erase work. During an erase, reads from any address of the chip return toggling status (DQ6). | done (`55c081e3e6d`, `061407363b6`) | `intelfsh::read_full` |
| S10.3 | Byte program can only clear bits; setting bits needs an erase. | done (`aa2de25502a`, `061407363b6`) | `intelfsh::write_full` |
| S10.4 | Flash contents persist as NVRAM, kept per machine and per BIOS (`nvram/<machine>[_<bios index>]/flash_even`, `flash_odd`). | done | MAME NVRAM |
| S10.5 | The flash is filled from the selected BIOS only when it has no NVRAM. An erase done by software persists, as on the board. | done (`7b135b0c754`) | `split_bios_for_flash` |
| S10.6 | A status read touches only the chips for the bytes being accessed. | done | `flash_r` |
| S11 | The `RESET` instruction resets the DUART, so `updateflash` can reboot into new firmware. | done (`bdcfddcb441`) | `reset_line_w` |
| S12 | The Docker image builds from a checkout that was also built on the host. It ships the ROM sets and the test scripts, and runs headless. | done (`2e2dd0d6b3b`) | `Dockerfile`, `.dockerignore` |
| S12.1 | In the container, `rosco-test -q` quickloads a program on the built-in firmware. | done | `scripts/rosco-test.sh` |
| S12.2 | In the container, flash NVRAM is written under `/work/nvram`, so it persists when `/work` is mounted. | done | image `WORKDIR` |
| S13 | `scripts/smoke-test.sh` boots every machine and checks the banner. It and `scripts/rosco-test.sh` use a fresh NVRAM directory per run. | done | `scripts/` |
| S14 | Known limitation, not a requirement: the 020/030 firmware speed check reads about 2× the clock, because Musashi doesn't model cache or prefetch timing. | accepted | README |
