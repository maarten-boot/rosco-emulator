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
| Planning docs and test helpers | — | `7762bf19035`, PR #7 | all |
| Merge flash ROM work into `master` | S10, S11 | PR #6 | T10–T15 |
| Crosscheck of Spec/Plan/Tests against `master` `038df42e8be` | all | branch `docs/crosscheck-2026-10-30` | T1, T4, T6, T10–T15 re-run |
| Tag `v0.10.0` (annotated) on the PR #6 merge `cb1cfe0b0df`; pushed. GitHub release and Docker Hub image not confirmed yet | S10, S11, S12 | tag `v0.10.0` | — |

## Open

In rough order of value.

1. **Re-run T16 (`updateflash`)** against `master`. PR #6 was merged
   without it, and the flash seeding changed (`7b135b0c754`) after it last
   passed. This needs the firmware tooling of the `../rosco-m68k` session.
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
11. **`rosco-m68k-emulation-report.md` is out of date** (a 2026-10-23
    snapshot). It still describes a read-only ROM, and its coverage table and
    line numbers predate the flash work. Either mark it as a snapshot that
    points here, or update §2 and §6.

12. **Confirm the `v0.10.0` release.** No GitHub release has appeared yet,
    and this session's token can't read Actions runs. Check that
    `release.yml` ran; if Actions is disabled, enable it and push the tag
    again.

## Next step

Item 1: re-run `updateflash` against `master` and record the result as T16.
Crosscheck again after the next functional change.
