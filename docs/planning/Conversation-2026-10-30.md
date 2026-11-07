# Conversation log — 2026-10-30

Continuation of the 2026-10-23 session.

1. **Request:** status?
   - Everything from 2026-10-23 is merged (`origin/master` at `423364ea807`).
   - The checkout is on a new branch, `feature/sst-flash`. Its five flash
     commits were made by the session in `../rosco-m68k`.
   - Pointed out that the global instructions ask for planning docs, and
     that `docs/planning/` doesn't exist yet.

2. **Request:** review the `sst-flash` commits; remove the old Docker image
   and build a fresh one.
   - Removed the old image and started the rebuild.
   - Review findings:
     1. The README is wrong: `-bios` still switches firmware, because NVRAM
        is kept per BIOS.
     2. The flash was re-filled at every reset when blank, so an erase done
        by software didn't stick.
     3. The `intelfsh` changes applied to every SST part, not just the
        39SF chips.
     4. `RESET` resets only the DUART.
     5. Debugger reads toggle DQ6 during an erase.
     6. The 68030 is about 20% slower (68010 unchanged).
   - Verified finding 1 by experiment, and finding 6 by speed measurement
     against `master`'s driver.
   - New image: smoke test 5/5, quickload PASS, NVRAM in `/work/nvram`.

3. **Request:** fix 1, 2 and 3 on `feature/sst-flash`.
   - Finding 2: the driver now splits the selected BIOS into `flash_even` /
     `flash_odd` ROM regions in `machine_start()`. `intelfsh` loads its
     region only when it has no NVRAM. Startup order checked in
     `machine.cpp`: ROM load, then device start, then `nvram_load`.
   - Finding 3: both SST checks are limited to device IDs `0xB5`–`0xB7`.
   - Finding 1: README rewritten. The ROM comment block notes that a
     replaced image needs the NVRAM deleted.
   - Tested with hand-assembled flash programs on 010 and 030: erase,
     program, AND, IDs, persistence, chip erase sticks, fresh NVRAM
     reseeds. Smoke test 5/5. `-bios v250/v242/v250` each boot correctly.
   - Commits `061407363b6`, `7b135b0c754`, `cec16b598fa`. The user's first
     push didn't reach GitHub; the second did.

4. **Request:** rebuild the Docker image.
   - Built `94cca3abb590` from `cec16b598fa` in 17¾ minutes. In the
     container: smoke test 5/5, quickload PASS, the flash tests give the
     same results, and NVRAM persists under the mounted `/work`.

5. **Request:** create the `docs/planning` files.
   - Branch `docs/planning` from `origin/master`. Added `Spec.md`,
     `Plan.md`, `Tests.md`, these logs, and `tests/` (`mkprogs.py`,
     `dbgrun.sh`, `sock_client.py`).
   - The `berr.bin` and `echo.bin` sources were rebuilt from the session,
     because the scratch copies had been cleaned up. Re-ran them through the
     new helpers: same results. The `dbgrun.sh` dump was widened to 24 bytes,
     so the 68030 fault address (offset 16) is included.

6. **Request:** status?
   - `feature/sst-flash` (8 commits) and `docs/planning` (1 commit) are
     pushed but not merged. The Docker image is current.

7. **User:** merged `docs/planning`.
   - Found that `feature/sst-flash` had been merged as well (PR #6, then
     PR #7). `master` is at `038df42e8be`.
   - T16 (`updateflash`) was not re-run before that merge.

8. **Request:** do the crosscheck.
   - Updated local `master`, then branched `docs/crosscheck-2026-10-30`.
   - Outside `docs/` and the `!/docs/` line in `.gitignore`, the code on
     `master` is identical to the tested `cec16b598fa`.
   - Located every Spec item (S1–S14) in the code. No contradictions.
   - Rebuilt the host binary from `master`, then re-ran:
     - T1 (5/5)
     - T4 (PASS)
     - T6 on all four CPUs (the 000 and 020 had last run on 10-23)
     - T10–T14 on 010 and 030
     - T15

     All passed. The rebuild was needed because the binary on disk had been
     linked at 17:44, while the checkout was on the pre-flash
     `docs/planning`.
   - Docs updated:
     - Spec: S10/S11 are on `master`; crosscheck note added.
     - Tests: last results updated.
     - Plan: merge done; T16 is now open item 1; new item 11 for the
       outdated report.

9. **Request:** tag the emulator merge (`dece8a1ff7c` was named).
   - `dece8a1ff7c` is the merge into `rosco-m68k-ram-bios` and is not on
     `master`; its tree is identical to `cb1cfe0b0df`, the PR #6 merge on
     `master`. The user chose `cb1cfe0b0df`, version `v0.10.0`, annotated.
   - Created the tag locally; the user pushed it.

10. **Request:** status?
    - The tag is on GitHub (`6fc2af26…` → `cb1cfe0b0df`), and the crosscheck
      branch is pushed but not merged.
    - No `v0.10.0` GitHub release yet. The Actions API returns 0 runs and a
      403 for this token, so whether `release.yml` ran is unknown (Plan item
      12).
