# Switch bring-up: 2026-11-04

The first diagnostic NRO builds. The full game engine does not yet build or
run on Switch. No hardware execution or deployment has been performed for
this repository.

## Actual ARM64 compilation audit

`scripts/switch/audit_compile.py` reads the existing PC compile database and
checks the actual generated game sources, native overrides, bridge consumers,
entry point and shared host sources with devkitA64 GCC 16.1.0 and GNU C++26.
It retains per-unit definitions, includes and forced headers, removes Windows
and Tracy definitions, and replaces desktop compiler/code-generation options
with explicit ARM64 options. It does not use `-fpermissive` or placeholder
implementations. Each result records the source hash, command and diagnostic.

The corrected full pass uses `-fsigned-char`: PS2/native-PC `s8` is plain
`char`, while ARM GCC's default plain char is unsigned. Three initial
narrowing failures disappeared with the correct signed-char ABI. The probe
also has compile-time checks for signed `s8` and 64-bit pointers.

| PC target family | Parsed successfully | Audited |
| --- | ---: | ---: |
| Recovered game units (`ct_ps2`) | 117 | 149 |
| Native game/platform overrides (`ct_port`) | 174 | 225 |
| Archive bridge consumers (`ct_port_bridge`) | 1 | 3 |
| Shared host sources (`ct_host`) | 43 | 44 |
| Game entry point | 0 | 1 |
| Total | 335 | 422 |

The 87 failures group by their first reported error:

- 53 require `sys/mman.h`, mostly through the Quill logger; the desktop arena
  memory implementation also requires it.
- 32 expose generated-source `extern`/`static` linkage conflicts that the PC
  Clang build accepts and GCC rejects.
- One requires desktop dynamic-loading header `dlfcn.h`.
- One, the cooperative game-thread implementation, requires `ucontext.h`.

These are syntax checks, not a percentage of game-port completion. They borrow
the PC tree's generated sources and header-only dependencies. They do not
establish Switch SDL3, Vulkan, FFmpeg, Rust bridge or other library availability,
successful code generation for every passing file, runtime ABI correctness,
or full-engine linking. Later errors may be hidden behind each first error.
The audit covers the game/host targets, not all third-party implementation
sources or the C++ test target. GCC handling of recovered functions that fall
off a non-void return also needs a decision: the PC uses Clang's
`-fno-strict-return`, which this audit does not reproduce.

Local detailed evidence is in `port/build/switch-audit-signed-char/report.json`
and its adjacent per-unit logs. The initial unsigned-char pass is retained in
`port/build/switch-audit` for comparison.

## Core math diagnostic

`switch_core_probe` compiles and links these complete real source units:

- PC-generated `ps2_src/mg_math.cpp`, produced by the existing override pipeline.
- Native `port/src/mg_math.cpp`.
- Kazarov `src/cpp/sce/libvu0.cpp`.

A small libnx console driver calls normalization, matrix translation and
recovered game interpolation routines, compares their results with known
values, displays PASS/FAIL, and returns to Homebrew Menu on +. The probe has
no retail asset dependency. The expected comparisons have not yet been run
on hardware.

Build validation confirmed an AArch64 ELF64 position-independent executable,
successful `elf2nro` packaging, and retained linked symbols for
`mgNormalizeVector`, `mgVectorInterpolate`, `sceVu0ApplyMatrix` and
`ctSwitchMathChecks`. Unused sections are collected at link time; compiling a
whole unit does not establish that every routine from it is runnable.

The NRO is 215,237 bytes. `build.json` records source hashes, compiler version,
PC database hash, commands, required symbols and the output SHA-256. Build
output stays ignored under `port/build`. Source and the pinned Kazarov
dependency remain unchanged by the probe.

## Reproduce

First configure and build the PC target so its source/header generation and
dependency acquisition have completed. On Windows use native Windows Python
and CMake ahead of MSYS on PATH. The local SDK is `C:/devkitPro`.

```powershell
python scripts/switch/audit_compile.py --output port/build/switch-audit-signed-char
cmake -S . -B port/build/switch -G Ninja -DPLATFORM=SWITCH
cmake --build port/build/switch --target switch_core_probe
```

The audit returns status 1 when any unit fails; its report is still written.
`--objects --match mg_math --output port/build/switch-math-objects` emits actual
ARM64 objects for a bounded code-generation check instead of only parsing.

Set `CT_SWITCH_PC_BUILD`, `CT_SWITCH_DEVKITPRO`, `Python3_EXECUTABLE` and
`CMAKE_MAKE_PROGRAM` explicitly if tool discovery needs them. A direct build
is also available:

```powershell
python scripts/switch/build_core_probe.py
```

The CMake output is
`port/build/switch/core-probe/chronicletwo-core-probe.nro`; the direct script
defaults to `port/build/switch-core-probe`. This environment required running
CMake/Ninja outside the sandbox to avoid a process-management stall; the
direct compiler and packer also worked inside it.

## Next implementation work

1. Separate the logging interface from Quill's desktop allocation dependency
   and add a real Switch logging backend. Recheck the 53 blocked consumers.
2. Implement the existing arena memory contract with native Switch allocation
   and protection, and preserve the game's cooperative scheduler with an
   ARM64 context backend. Verify guard pages and context switches on hardware.
3. Resolve GCC's generated-source linkage conflicts in the generation layer
   or validate an alternative Clang cross-toolchain. Preserve shared gameplay
   definitions and symbol aliases; do not edit retail source to hide failures.
4. Establish ARM64 builds of the archive bridge, scripting and decoding
   dependencies. The installed Rust compiler lists
   `aarch64-nintendo-switch-freestanding`, but the required target libraries,
   standard library and C++ bridge have not been built or linked for it.
5. Map Kazarov's renderer interface to deko3d, then connect input, audio output
   and SD filesystem services and boot through the shared `MainLoop`.

The [PC baseline report](PC_BASELINE_2026-11-04.md) records existing game
checkpoints, completed CPU checks and limitations for later comparisons.
