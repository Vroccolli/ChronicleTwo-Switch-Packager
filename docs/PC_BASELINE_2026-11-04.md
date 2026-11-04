# PC baseline: 2026-11-04

## Source and environment

- ChronicleTwo base: `cb910edeea375887aab8e00085b93150be4e081e`.
- Kazarov: `14164f13fb4fcd4395009b81ba3f3208df1e52b3`, unchanged.
- Target: native Windows x86-64 Release, Clang 23.1.2 (llvm-mingw 20260922),
  Rust 1.99.0 gnullvm, Ninja 1.13.2, SDL3 3.4.18, Vulkan SDK headers/loader
  1.4.363.0, CMake 3.29.2 and Python 3.12.14.

## Setup corrections

The inherited FFmpeg 2026-10-13 archive returned HTTP 404. Windows acquisition,
configuration and dependency markers now use the official LGPL shared FFmpeg
9.0.2 build `45-g1e65c6cd5d`, [official release `autobuild-2026-11-03-12-50`](https://github.com/BtbN/FFmpeg-Builds/releases/tag/autobuild-2026-11-03-12-50), verified
against the GitHub asset SHA-256:
`be035a7f1763fdab8430949bed65eab94dc91cbdea1d03607e4ac5403e73ce76`.

Rust acquisition now explicitly uses Windows `System32/tar.exe`: the MSYS tar
on this machine interpreted the `C:` archive path as a remote hostname.
Build commands also put native Windows CMake and Python before MSYS on PATH;
MSYS CMake otherwise treated the Windows source directory as a relative path.
These are build setup changes; gameplay sources have not been changed.

## Checks completed

- `tools/windows/test-build.ps1`: passed on PowerShell 7.6.5.
- Python `unittest discover -s scripts/port -p 'test_*.py'`: 212 tests,
  successful with four skips. Run outside the sandbox because fixture cleanup
  and child-directory access failed under sandbox restrictions.
- `scripts/port/native_gate.py --binary ... --nm .../llvm-nm.exe`: runtime
  source and linked executable audits passed.
- Native Windows Release game and `dc2data` extractor: built successfully.
  Initial compilation took 595.6 seconds; configure plus compilation took
  638.7 seconds after toolchain acquisition.
- `dc2data_bridge` CTest case: passed (7.67 seconds).

## Early runtime finding

The game prints usage for `--help` but remains alive. A retry outside the
sandbox also exceeded a 20-second timeout and was terminated. This is an
inherited early-exit/shutdown issue; its cause has not been established.

## Runtime procedure

`scripts/port/run_pc_baseline.ps1` runs existing input fixtures for title,
costume selection, opening combat, Palm Brinks movement and dungeon entry.
Each case gets a fresh save/configuration root, bounded frame count, timeout,
stdout/stderr logs and a final PNG. It renders offscreen through the real
Vulkan renderer. Successful exit and capture creation require separate visual
review and do not establish interactive input, audible output, full gameplay
correctness or campaign completion.

## Recorded runtime results

The PAL disc extracted and verified successfully in 14:54. The publisher
checked 307,847 verification entries and published a 3.8 GB archive containing
91,149 entries. Assets remain in the ignored `port/run` directory.

Offscreen rendering used the NVIDIA GeForce GTX 1650 with Vulkan 1.3.289.
The integrated Intel device was rejected because it exposed Vulkan 1.2.
All five initial bounded runs exited with status 0 and produced PNG captures:

| Run | Frames | Seconds | Visual review |
| --- | ---: | ---: | --- |
| Normal boot/title | 2,800 | 18.29 | Title logo during fade; later 3,900-frame run shows the complete title |
| Initial costume fixture | 3,900 | 8.47 | Still at title; costume selection not established |
| Opening debug jump | 4,200 | 15.43 | Monica, Griffon Soldiers, lock-on and combat HUD visible |
| Palm Brinks debug jump/walk | 480 | 6.51 | Max and town scene rendered |
| Underground Channel debug jump/entry | 2,400 | 16.21 | Max, dungeon HUD/map, sewer geometry and water rendered |

Evidence is under `.kazarov/baseline/2026-11-04-initial`; later title/New Game
attempts are in the neighboring `2026-11-04-newgame*` directories. Revised
scripted Start presses still stopped at title. A windowed `--jump title` run
displayed the New Game/Continue/Options menu, but interactive New Game
confirmation was not established before the user stopped Computer Use.

The complete non-GPU CTest selection finished: 1,604 cases, initially 1,559
passed, 41 skipped and three timed out. An isolated retry passed all three:
`PalTiming.Sites`, `ScriptTimingStartup.HeadlessMismatchExitStatusDefault`
and `ScriptTimingStartup.HeadlessMismatchExitStatusLegacyPal`. Combined result:
1,562 passed, 41 skipped, no remaining failures. Data-dependent skips mean
this does not establish that every disc-backed test ran. GPU-labeled unit
tests were excluded; the rendered scene replays exercised the actual GPU.

Run the prepared interactive baseline with `./run-pc.ps1`. It uses `port/run`
for assets and ordinary saves; replay saves remain separate. This baseline
does not establish full campaign completion or audible output correctness.
