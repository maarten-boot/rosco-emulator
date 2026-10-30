# Conversation log — 2026-10-23

Session with Claude Code (Opus 5.5) in `rosco-emulator`, starting on branch
`rosco-m68k-ram-bios` at `f283c471eda`. This log was written up on
2026-10-30, from the session.

1. **Request:** inspect the repo and write a report on the rosco_m68k
   emulation in `./tmp/`.
   - Read the driver (`src/rosco/drivers/rosco_m68k.cpp`), README, ROM
     provenance, scripts, CI and logs.
   - Smoke test: 4/4 m68k machines pass. `-ram 1m/5m/14m` reports the right
     sizes with both BIOSes. The 020/030 read 42.8/45.7 MHz (known
     limitation).
   - Wrote `tmp/rosco-m68k-emulation-report.md`. Findings: quickload capped
     at 1 MB, wrong bus-error fault addresses, `FFFE00–FFFFFF` hole, OPCR log
     spam, LED stubs, duplicated code, comment nits, and a placeholder author
     on `e06207686e2`.

2. **Request:** fix the commit with the wrong author.
   - The commit was already on `origin/master` (PR #1). Asked how to handle
     that. The user chose to rewrite history locally.
   - `git filter-branch` on `master` and `rosco-m68k-ram-bios`: the author
     became Maarten Boot, and the message became "debugger gdbstub: Map
     m68010 to the m68000 register set". Backup branches were created.
   - The user force-pushed and then deleted the backups. Verified over HTTPS:
     no remote branch contains `e06207686e2` any more.

3. **Request:** fix the quickload and bus-error findings.
   - Confirmed in `emumem_hedp.cpp` that handler offsets are relative to the
     range start.
   - Templated the handlers on their range base (`unmapped_r/w<Base>`), with
     +1 for a low-byte access. Fixed the inverted read flag passed to
     `set_buserror_details()`. Quickload now uses `m_ram->size()`.
   - Verified with a hand-assembled bus-error program on all four CPUs
     (before/after table), a 900 KB quickload with `-ram 1m/2m`, and the
     smoke test (5/5).
   - Committed `0c155ca9cca`, plus `3b49f867ab6` (ignore `tmp/`). These were
     first committed on `master` by mistake (the checkout had changed), then
     moved to the feature branch before any push. The user pushed and merged
     them as PR #2.
   - Updated the report, which the user had added to the repo, in
     `bdbcd27880f`.

4. **Question:** does a Docker image make sense, with the built-in ROM and a
   program to load?
   - The `Dockerfile`, entrypoint and release workflow already exist.
   - Tested on the host: `rosco-test -q hello.bin` passes, and the console
     works over TCP (`-bitb socket.127.0.0.1:4321`) with an echo program.
   - The first `docker build` failed: `COPY . .` brought in the host-built
     `3rdparty/genie/bin/linux/genie`, which needs glibc 2.38, but the
     builder image has 2.35. Fixed in `.dockerignore`.
   - In the image: smoke test 5/5, quickload PASS, socket console works if
     you connect about 1 s after start (Docker's proxy accepts early
     connections and then drops them).
   - Committed `2e2dd0d6b3b`. The user pushed and merged it (PR #5).
