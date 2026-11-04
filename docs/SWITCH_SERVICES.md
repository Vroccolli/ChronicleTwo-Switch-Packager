# GCC linkage and controlled-crash services

This checkpoint prepares strict GCC build copies and provides a bounded Switch
crash backend. The native PC and recovered PS2 sources keep their existing
linkage declarations; the pinned Kazarov submodule is unchanged.

## Generated GCC sources

`scripts/switch/prepare_sources.py` reads each unit's stub header and removes
file-scope `static` only from names explicitly exported with an assembly label.
It uses the existing override parser to distinguish definitions/prototypes from
comments, strings, members and function-local statics. Prototypes and definitions
receive the same linkage. Assembly labels and function bodies are preserved.
The output includes a manifest of original/generated/stub hashes and every edit.

Three additional fixes are explicit and checked against the expected input:

- `mainloop.cpp` already declares `MainBuffer` extern before its static
  definition; its generated definition now agrees with that declaration.
- `menuaqua.cpp` explicitly converts the two high-bit debug password bytes to
  signed char, preserving their byte patterns without narrowing errors.
- `menusys.cpp` copies the integer cursor directly. The original expression
  reads it through `void **`, treating four-byte integer storage as an eight-byte
  pointer and then truncating it. The generated fix copies the intended value.

These changes apply to Switch build copies after the normal PC override pipeline.
No blanket permissive option, global `static` removal or error suppression is used.

## Crash backend and limits

`port/switch/crash` replaces the desktop capture/report/symbol units when
`--services` is selected. Controlled failures, Kazarov fatal hooks and
`std::terminate` write a synchronous text report and exit. Reports include the
failure kind/message, thread identity, shared game-state context and raw stack
addresses with module offsets. The SDK linker supplies the image/code bounds.
Out-of-image addresses are retained as raw addresses rather than named as game
functions. Use the exact saved ELF and `aarch64-none-elf-addr2line` offline.

The backend reuses the existing context storage, bounded text formatting and
path redaction. Formatting helpers moved from `crash_report.cpp` to
`crash_common.cpp`, shared with the PC backend. A bounded logger flush avoids
re-entering the synchronous logger during a delivery callback.

Raw CPU faults, direct `abort()`, guard violations and stack exhaustion remain
with the system handler. This checkpoint does not claim a Horizon exception
handler, fault-register capture, full unwinding across assembly fibers, runtime
DWARF symbolication or a crash dialog. Unlike the desktop bundle writer, it keeps
one `crashes/switch-last-report.txt`, overwritten by the next report. SD errors
fall back to stderr; a working heap/filesystem is required for controlled reports.

## Commands and evidence

```powershell
python scripts/switch/audit_compile.py --services --jobs 4 --output port/build/switch-audit-services
python scripts/switch/link_engine.py --jobs 4
cmake --build port/build/switch --target switch_services_probe
python scripts/switch/test_prepare_sources.py
```

The engine command emits actual ARM64 objects, then attempts to link them with
libnx. It records compiler failures and unresolved symbols in
`port/build/switch-engine/link-report.json` and `link.log`. Desktop third-party
binaries are never substituted for Switch libraries. A failed link is not
packaged or deployed as a game.

Configure `scripts/switch/services_tests` with the pinned native LLVM compiler
and Ninja, then build and run CTest. Its six child-process scenarios check
synthetic writing, a controlled fatal exit, terminate, the fatal hook, a worker
failure and stderr fallback without a bundle root. They inspect exit status,
persisted text, context, path redaction and raw frames. Four Python tests check
the linkage transform's scope boundaries and assembly export discovery.

The services diagnostic 0.4.1 includes the previously verified runtime checks.
It writes a synthetic report while staying alive, then displays three service
rows for readback, context/redaction and raw frames/module offsets. Expect all
rows PASS and Services, Runtime and Logging failure bits all `0x00`.
The report is under
`sd:/switch/ChronicleTwo-PC-Switch/services-probe/saves/crashes/switch-last-report.txt`.
Close the previous probe fully before launching the services probe, and press +
to return to Homebrew Menu after taking a screenshot.

## First hardware result and correction

The 0.4.0 capture `2026110417444000-5361907E67981C0712C80F39FBC192A1.jpg`
(console timestamp 2026-11-04 17:44:40) shows report persistence and context
redaction PASS, with Services failure bits `0x04`. Math, logging and all eight
runtime checks pass. The retrieved report contains four raw frames but a zero
module base, making its relative addresses incorrect.

The SDK linker marks `__start__` as an absolute zero constant, whereas `_start`
is the actual text entry at image offset zero. Version 0.4.1 obtains `_start`
through PC-relative AArch64 instructions so the loaded image address includes
ASLR. The base check and offset assertions are retained. The original report,
capture and matching 0.4.0 ELF/build manifest are preserved under
`port/build/switch/services-probe/hardware`. Corrected hardware execution is
still pending.

## Completed build and link checks

The final services audit passes **426/426 syntax checks and 426/426 actual
ARM64 object compilations**. This includes the additional shared formatting
unit and fiber source; the number is not a percentage of game-port completion.
All recorded source hashes were checked after the version correction.
The PC game and test executable rebuild successfully. All 13 existing crash
unit tests and seven desktop crash integration checks pass. The six focused
Switch-backend host scenarios and four source-transform tests also pass.

The real engine link fails with 299 distinct unresolved symbols:

| First symbol family | Unresolved names |
| --- | ---: |
| Vulkan | 105 |
| SDL3 | 83 |
| Luau | 44 |
| FFmpeg | 39 |
| Rust/CXX bridge names | 11 |
| Other names | 17 |

The other names comprise seven fastgltf symbols, six Kazarov archive wrapper
functions, three iconv functions and one Tracy thread-naming function.
The link also finds a duplicate `__assert` definition between the recovered
game runtime and newlib. These remain open integration work; no failed engine
ELF is retained as a runnable artifact. Renderer replacement and matching
Switch third-party libraries are required before game boot.

Version 0.4.1 is uploaded to
`/switch/ChronicleTwo-PC-Switch/chronicletwo-services-probe.nro`.
Readback matches 1,177,797 bytes and SHA-256
`1d4bc0d6702cd4040209e83786467c877b9f751ebc9e6034edf491151466abb7`.
Its disassembly resolves `_start` with `adrp`/`add`, without the old absolute
symbol's zero-valued GOT entry. The hardware retest is pending.
