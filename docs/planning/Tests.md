# Tests — rosco_m68k emulation

Each test traces to a `Spec.md` item. "Last result" is the most recent run,
with the date and the code it ran against.

## Setup

```sh
make -j"$(nproc)"                                  # host binary ./rosco
P=$(mktemp -d)                                     # scratch directory
python3 docs/planning/tests/mkprogs.py "$P"        # hello/echo/berr/flash1/flash2.bin
```

The helpers in `docs/planning/tests/`:

- `mkprogs.py` writes hand-assembled 68k programs. They are linked for the
  quickload address `0x40000` and use no firmware calls.
- `dbgrun.sh MACHINE NVRAM_DIR ADDR [PROGRAM]` runs the machine for 6
  emulated seconds, then prints 24 bytes at `ADDR` and how often the
  `CPU @` banner line appeared. It needs the `-debugger none` script fix
  (`9d9c8ed08b4`).
- `sock_client.py HOST PORT` checks the console socket with `echo.bin`.

T10–T16 need the `feature/sst-flash` code.

## Tests

| ID | Spec | Test | Expected | Last result |
| --- | --- | --- | --- | --- |
| T1 | S1, S2, S8, S13 | `scripts/smoke-test.sh` | All 5 machines PASS. | PASS, 2026-10-30, `cec16b598fa` |
| T2 | S5 | `rosco_m68k_010 -ram {1m,5m,14m}` with both BIOSes; read the boot banner. | `with 1048576` / `5242880` / `14680064 bytes RAM` | PASS, 2026-10-23, `f283c471eda` |
| T3 | S6 | Boot `-bios v250`, then `v242`. | Banner shows `Classic 2.50.DEV`, then `Classic 2.42`. | PASS, 2026-10-30, `cec16b598fa` |
| T4 | S7 | `scripts/rosco-test.sh -m rosco_m68k_010 -q $P/hello.bin -e 'hello, world'` | PASS | PASS, 2026-10-30, `cec16b598fa` |
| T5 | S7 | Pad `hello.bin` to 900 KB, then quickload it with `-ram 1m` and with `-ram 2m`. | `1m`: "Invalid image length". `2m`: loads and runs. | PASS, 2026-10-23, `0c155ca9cca` |
| T6 | S4, S4.1 | For each `M` of `rosco_m68k_000/010/020/030` and `A` of `3000 3020 3040 3060 3080`: `dbgrun.sh M $P/nv-M A $P/berr.bin` | Fault address in each frame (see the note below the table): `00200001`, `00200000`, `00F00021`, `00F80060`, and on 020/030 also `02000000`. | PASS on 010/030, 2026-10-30. 000/020 PASS, 2026-10-23. |
| T7 | S4.1 | Same as T6 on the 68010, with the driver from before `0c155ca9cca`. | Shows the old bug: `00100000`, `00100000`, `00F00000`, `00F00000`. Regression reference only. | as expected, 2026-10-23 |
| T8 | S9 | `./rosco rosco_m68k_010 -quik $P/echo.bin -terminal null_modem -bitb socket.127.0.0.1:4321 -video none -seconds_to_run 15 &` then `sock_client.py 127.0.0.1 4321` | `got hello: True`, `got echo: True`, exit 0 | PASS, 2026-10-30 |
| T9 | S3 | Boot any machine. The firmware starts from the vectors at `E00000`, and quickloaded code that writes to `0`–`7` keeps running. | Banner appears. T4 passes. | PASS (via T1/T4) |
| T10 | S10.1, S10.2, S10.3, S10.6 | `dbgrun.sh M NV 3000 $P/flash1.bin` on 010 and 030 with a fresh `NV` | `0034abcd bfbfb7b7 cafef00d` | PASS, 2026-10-30, `cec16b598fa` |
| T11 | S10.4 | After T10, the same `NV` with no program: `dbgrun.sh M NV efe000` | `0034abcd ffffffff ...`, banner 1 | PASS, 2026-10-30 |
| T12 | S10.2, S10.5 | After T11, the same `NV`: `dbgrun.sh M NV 3000 $P/flash2.bin` | `ffffffff 00000000 cafef00d` | PASS, 2026-10-30 |
| T13 | S10.5 | After T12, the same `NV`: `dbgrun.sh M NV e00000` | Banner 0. The flash stays blank: the dump is all `ff` on 030, and the 010 halts (no dump). | PASS, 2026-10-30 |
| T14 | S10.5 | `dbgrun.sh M <fresh NV> e00000` | `000c0000 00e00404 ...`, banner 1 | PASS, 2026-10-30 |
| T15 | S10.4, S6 | With one `NV`, boot `-bios v250`, then `v242`, then `v250`. | Each boots its own firmware. `NV` contains `rosco_m68k_0x0` and `rosco_m68k_0x0_1`. | PASS, 2026-10-30 (020) |
| T16 | S11, S10 | `updateflash` from 2.42 to 2.42.1, then restart. | The new firmware boots right away and is still there after a restart. | PASS per `bdcfddcb441` message (other session). **Not re-run after `7b135b0c754`.** |
| T17 | S12, S12.1 | `docker build -t rosco-emulator:local .` on a host-built tree, then `docker run --rm --entrypoint rosco-smoke-test rosco-emulator:local` | Build succeeds. All 5 machines PASS. | PASS, 2026-10-30, `cec16b598fa` |
| T18 | S12.1 | `docker run --rm -v "$P:/work" --entrypoint rosco-test rosco-emulator:local -m rosco_m68k_010 -q hello.bin -e 'hello, world'` | PASS | PASS, 2026-10-30 |
| T19 | S12.2 | T10–T13 in the container, with `/work` mounted. | Same results. `/work/nvram/rosco_m68k_010/flash_{even,odd}` exist. | PASS, 2026-10-30 |
| T20 | S9, S12 | Container with `-p 4321:4321` and `-bitb socket.0.0.0.0:4321`. Connect after waiting 1 s. | hello and echo work. Connecting too early gets an immediate EOF from Docker's proxy. | PASS, 2026-10-23 |

**Frame layouts for T6.** The fault address sits in a different place for
each CPU:

- **68000** (group 0 frame): address at bytes 2–5, access word at bytes 0–1
  (bit 4 = read).
- **68010** (format `$8`): address at bytes 10–13, SSW at bytes 8–9
  (`0105` = read, `0005` = write).
- **68020/68030** (format `$B`): address at bytes 16–19, SSW at bytes 10–11
  (bit 6 = read).

## Gaps

- No test yet for S4.2 (the 16-cycle bus-error mask during read-modify-write)
  or for the SD card and IDE paths beyond the firmware's own probing.
- T16 should be re-run against the current `feature/sst-flash`.
- None of these tests run in CI; only `smoke-test.sh` (T1) does.
