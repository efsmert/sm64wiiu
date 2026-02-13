# SM64 Wii U Port Agent Guide

This file is project-specific guidance for agents working in `/Users/samiareski/sm64wiiu`.

## 1) Project context
- `sm64wiiu/`: shipping Wii U target. `sm64coopdx/`: donor project for enhanced features.
- Mission: port Co-op DX features into `sm64wiiu` while keeping Wii U bootable at each step.
- **IMPORTANT:** The core goal is to port all enhanced features from `sm64coopdx` into `sm64wiiu`. `sm64coopdx` targets desktop platforms (Windows/macOS/Linux), while `sm64wiiu` is the native Wii U port. Both are Super Mario 64 decompilation projects.
- **IMPORTANT:** When re-implementing a feature in `sm64wiiu`, first inspect the corresponding `sm64coopdx` code path to understand its wiring, then bring the implementation over as directly as possible, adapting only the platform-specific wiring differences required for Wii U.
- The project already compiles and runs. Main risk is Lua/mod runtime stability, not toolchain setup.
- Strategy: runtime and gameplay parity first, networking last.
- Focus outputs: `.rpx` and `.wuhb`.
- The project root contains a folder `flood-expanded-release/` which contains the mod data for the `Flood` mod. For easy access to debug the mod's behavior against our port, use that. This makes it so you do not have to search the computer for the files that are also in cemu's emulated sdcard.

## 2) First steps every session
1. Read `SUMMARY.md` — but read smart, not linearly. See section 7 below for how.
2. Check repo state (`git status`) and preserve in-flight work.
3. After major edits, use the project build script.

## 3) Build and artifact workflow

From repo root, run:

```bash
./build_wiiu_then_wuhb.sh
```

This script runs `make -C sm64wiiu -j4` first and runs `make -C sm64wiiu wuhb` only if the first command succeeds.

Run a clean build first in these cases:
1. First compile of a new session (to avoid stale artifacts from prior agent work).
2. Switching between debug and non-debug build configs.

Clean + rebuild commands:
```bash
make -C sm64wiiu clean
./build_wiiu_then_wuhb.sh
```

Clean + debug rebuild:
```bash
make -C sm64wiiu clean
WIIU_CRASH_DEBUG=1 ./build_wiiu_then_wuhb.sh
```

If the build fails, do not hide output. Show the terminal error output in full (or enough contiguous lines around the first error) so debugging can continue from the real root cause.

Expected outputs:
- `sm64wiiu/build/us_wiiu/sm64.us.rpx`
- `sm64wiiu/build/us_wiiu/sm64.us.wuhb`

### Crash-debug build mode (Wii U)

For crash investigation sessions, enable the Wii U crash-debug build profile:

```bash
WIIU_CRASH_DEBUG=1 ./build_wiiu_then_wuhb.sh
```

This enables debug-friendly Wii U compile/link flags (including map output) and prints `Crash Debug: yes` in the build options banner.

Additional debug artifacts generated in this mode:
- `sm64wiiu/build/us_wiiu/sm64.us.elf` (symbol-rich ELF for address resolution)
- `sm64wiiu/build/us_wiiu/sm64.us.wiiu.map` (linker map)

## 4) Wii U gotchas (only what matters here)

1. **Endianness** — Wii U is big-endian. Watch audio/network/binary data paths (`__builtin_bswap16/32`).

2. **GPU cache coherency** — CPU and GPU don't share coherent cache. Use `DCFlushRange`/invalidate for CPU-written GPU resources.

3. **Lifecycle stability** — ProcUI/WHB loop correctness is mandatory to avoid HOME/background crashes.

4. **Logging APIs** — `WHBLogPrint` is non-variadic. Use `WHBLogPrintf` for formatted messages.

5. **Lua on Wii U** — Lua startup/parser/runtime paths are sensitive. Keep current stability guards unless replaced by a proven safer design.

6. **Platform Specific** - For low-level Wii U platform APIs (GX2, AX audio, VPAD, networking), see `WIIU_REFERENCE.md`. Only needed when writing new platform-level code.

7. **Cemu diagnostics** — For crash/hang diagnosis, check the log after reproducing:
   ```bash
   tail -n 300 "$HOME/Library/Application Support/Cemu/log.txt"
   ```
   For symbolized crash triage, run:
   ```bash
   sm64wiiu/tools/wiiu_decode_cemu_crash.sh
   ```
   Optional arguments:
   ```bash
   sm64wiiu/tools/wiiu_decode_cemu_crash.sh [cemu_log_path] [elf_path]
   ```
   This script parses the latest `Error: signal` block (IP/LR/ReturnAddr) and resolves addresses to function/file:line using `powerpc-eabi-addr2line`.

## 5) Current porting priorities
1. Keep startup and gameplay stable on Wii U.
2. Expand gameplay/mod parity in validated slices.
3. Preserve donor compatibility where practical.
4. Defer full networking parity until runtime parity is stable.
**Be ambitous, this is a lot of work and will be a lot of porting. Don't Be afraid to make significant change if it's going to be worth it**

## 6) Working rules for this repo
- Keep `sm64wiiu` buildable at all times.
- Prefer incremental compatibility shims over large risky rewrites, unless the user suggests an overhaul to the codebase.
- Add comments for non-obvious Wii U behavior and stability workarounds.
- If runtime behavior changes, record validation and result in `SUMMARY.md`.

## 7) How to use `SUMMARY.md`

### Reading it

The summary is organized by topic, not chronologically. Here's the layout:

- **Sections 1–4** (scope, repo map, current state, build) — orientation. Read first.
- **Section 5** (implemented work, A through N) — each lettered subsection is a self-contained topic (e.g. "G) Lua + mods foundation", "N) Runtime stability hardening"). Find the subsection relevant to your task. You don't need all of them for every task.
- **Section 6** (active decisions) — read if you're about to change a stability-sensitive path.
- **Section 7** (gotchas) — skim before any session. Short and worth knowing.
- **Section 8** (remaining work) — check before starting new work to see what's already known to be incomplete.
- **Section 9** (milestone ledger) — chronological record. Useful for understanding what happened when, not for understanding current state.
- **Section 10** (update format rules) — read before writing to the summary.

### Writing to it

When you complete work, update `SUMMARY.md`. Follow the rules in its section 10. The short version:

1. **Update existing sections in place.** If you added Lua bindings, update section 5J. Don't create a new lettered section for incremental additions.

2. **Create a new lettered section only for genuinely new subsystems.** "O) DJUI text rendering pipeline" warrants a new section. Three more constants in the compatibility surface does not.

3. **Add one ledger entry per material milestone** using the template in section 10:
  ```
    ### YYYY-MM-DD
    - <short milestone name>: <what changed>
      - files: <key files>
      - validation: <commands run>
      - outcome: <result>
      - gotcha: <single concrete issue and mitigation>
  ```
    The gotcha is OPTIONAL, you should only include one if something important was learned

4. **Never repeat boilerplate.** Artifact paths are listed once. Build commands are listed once. Don't restate them per entry.

5. **Update sections 6/7/8** if your work changes active decisions, reveals new gotchas, or completes remaining work items.

### Self-improvement

When you discover a repeated correction or platform-specific lesson, codify it:
- Add it to the relevant section of `SUMMARY.md` (gotchas, decisions, or inline in the work section).
- If you use knowledge from a previous agent's documented gotcha, mention it so the user knows the documentation is working.
- You can modify `SUMMARY.md` without prior approval as long as you follow the format rules.

The goal: avoid re-learning the same lessons across sessions.
