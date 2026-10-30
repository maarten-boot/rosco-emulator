# Plan — rosco_m68k emulation

Status as of 2026-10-30. Spec items are in `Spec.md`, tests in `Tests.md`.

## Done

| What | Spec | Commit / PR | Tests |
| --- | --- | --- | --- |
| Repository and emulation report (`rosco-m68k-emulation-report.md`) | — | `81a3981a36c`, `bdbcd27880f` | — |
| Replace the placeholder author on the gdb stub commit (rewrite + force push) | — | `93e0ae532cc` | — |
| Real bus-error fault address on 010/020/030; quickload up to the end of `-ram` | S4.1, S7 | `0c155ca9cca`, PR #2 | T5, T6, T7 |
| Ignore `tmp/` | — | `3b49f867ab6` | — |
| Docker build on a host-built tree (`.dockerignore`) | S12 | `2e2dd0d6b3b`, PR #5 | T17 |
| Docker checks: quickload, socket console, NVRAM in `/work` | S9, S12.1, S12.2 | — | T18–T20 |
| Flash ROM as 2× SST39SF040, `RESET` resets the DUART (other session) | S10, S11 | `55c081e3e6d`…`aa2de25502a` | T10–T16 |
| Review fixes on `feature/sst-flash`: SST changes limited to the 39SF parts, flash seeded only without NVRAM, README per-BIOS NVRAM | S10.2, S10.3, S10.5 | `061407363b6`, `7b135b0c754`, `cec16b598fa` | T10–T15 |
| Planning docs and test helpers | — | branch `docs/planning` | all |

## Open

In rough order of value.

1. **Merge `feature/sst-flash`** into `master` (it's pushed). Before that,
   re-run T16 (`updateflash`) against `cec16b598fa`.
2. **`FFFE00–FFFFFF` returns 0 without a bus error**, and the 020/030 maps
   end at different addresses. Check the board's address decoding before
   changing anything. (Report §5 item 3.)
3. **`RESET` resets only the DUART.** Check the schematic for whether the
   reset line also resets ATA and the SD card, and re-arms the boot-vector
   overlay (S3, S11).
4. **Debugger reads during an erase toggle DQ6.** `intelfsh::read_full`
   ignores `side_effects_disabled()`, so a debugger view of the ROM can
   disturb the firmware's status polling.
5. **DUART `Unhandled OPCR value: 08` log spam.** Silence it, or tell the
   firmware authors (OP3 TxCB clock vs C/T output).
6. **LEDs**: export `write_red_led` / `write_green_led` as outputs, so tests
   can see them.
7. **Second SPI device (OP7 `SPI_CS1`)** is not wired.
8. **Duplicated per-CPU code**: the four copies of the boot-vector and
   cpu-space handlers could become a template on the data width.
9. **68030 speed**: the flash handler costs about 20% (690% → 551%).
   Optional fast path: map the ROM directly while both chips are in
   read-array mode.
10. **Tests**: add T6/T10–T14 to CI as a script, and add a test for S4.2.

## Next step

Item 1: re-run `updateflash` on the current `feature/sst-flash`, then
merge. After that, crosscheck `Spec.md`, `Plan.md`, `Tests.md` and the code
again.
