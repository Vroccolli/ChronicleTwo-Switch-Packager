# Switch logging checkpoint: 2026-11-04

The logging dependency blocker is removed. A real synchronous Switch backend
now implements the existing logging API, and the logging diagnostic builds
as an ARM64 NRO. Version 0.2.0 was uploaded and tested on hardware; its
readback check failed as described below. The corrected 0.2.1 probe was uploaded,
verified by size and SHA-256, and passed the hardware retest.
Full game boot remains outside this checkpoint.

## Verified hardware retest

Album capture `2026110416552000-5361907E67981C0712C80F39FBC192A1.jpg`
(console timestamp 2026-11-04 16:55:20) shows all seven logging rows PASS,
`Logging failure bits: 0x00`, successful 3,389-byte readback, math PASS and zero
failed math comparisons. Retrieved log
`darkchronicle-20261104-165516-6188.log` is exactly 3,389 bytes and contains
all required markers, all 32 distinct worker records, and no filtered messages.
The readback failure is resolved for this tested Switch run. Return-to-menu
behaviour was not independently observed in the capture.

The device's capture/session clock is recorded as shown, without assuming it
matches the host clock. The verified uploaded NRO SHA-256 is
`c815d027317a0401a19883228d31ececf129abcee63ebff02d552bd083c12e8f`.
`hardware/validation-0.2.1.json` records the bounded hardware result alongside
the preserved capture and log.

## First hardware result and readback correction

The console's Album capture `2026110416480900-5361907E67981C0712C80F39FBC192A1.jpg`
(console timestamp 2026-11-04 16:48:09) shows six logging checks passing,
session-file readback failing with `0x40`, and all math checks passing.
The retrieved `darkchronicle-20261104-164803-b750.log` contains every required
persisted marker and no filtered messages. File writing and worker delivery
therefore have direct hardware evidence despite the probe's readback failure.

The original probe attempted a second read handle while its append writer was
still open. Version 0.2.1 disables file logging through the existing API before
opening the independent reader, preserving all marker assertions, and prints
the reader's open status, byte count and errno. This addresses a likely handle
sharing/visibility issue; the precise original open/read failure was not
instrumented. The subsequent hardware retest above confirms that closing the
writer before readback resolves the probe failure on the tested console.
All 30 focused host checks pass after the correction. Captures, the retrieved
log and `validation.json` are under the ignored
`port/build/switch/logging-probe/hardware` directory. No files on the console
were changed while fetching this evidence.

## What changed

- `port/src/log/log.hpp` selects a backend without including Quill on Switch.
  The desktop queue/macro implementation moved intact to `log_quill.hpp`;
  its backend remains in `log.cpp` and is excluded on Switch.
- `log_common.cpp` holds the shared category/level names, spec parsing,
  override precedence, retention and PS2 printf line assembly.
- `log_sync.hpp` and `log_sync.cpp` implement Switch filtering, formatting,
  serialized console/file writes, early file buffering, retention, the file
  soft cap, per-thread rate limits, capture hooks, Kazarov routing, flushes,
  shutdown and late records. No gameplay functions were replaced.
- State survives process/static teardown. A crash flush before initialization
  returns false without allocating. Capture callbacks run outside the I/O
  mutex and recursive log calls are suppressed.

Switch formats and writes on the caller. This backend can allocate and block
on SD/console I/O; it does not have PC's asynchronous audio-callback performance
guarantee. Crash-flush budgets bound mutex acquisition, not a storage driver's
flush duration. Native crash handling and symbol resolution still need their
own Switch implementation.

## Verification

The focused host CTest run passed **30/30**, with one process per case as in
the main repository:

- 12 existing PC/Quill logging cases.
- Those same 12 cases against the synchronous backend.
- Five synchronous-backend cases for filtered argument evaluation, capture
  reentrancy, late/partial records after shutdown, file-open failure, and
  crash flushing before initialization.
- The exact hardware diagnostic workload in a fresh host process: formatting
  and filtering; boot/category precedence; rate limits; printf/Kazarov routing;
  two worker threads; shutdown, late records and flushes; and session-file
  readback. All seven workload rows reported PASS.

The initial combined-process runner exposed test-state contamination after
the intentional failed-file attachment. The final runner uses the repository's
documented process-lifetime logger isolation; no assertions were weakened.
The two config-JSON cases in `log_test.cpp` are excluded from this focused
standalone target; configuration storage was not changed.

The full native PC game rebuilt and linked successfully. A 120-frame offscreen
title-scene smoke run exited 0, rendered the title background, and produced
its session log with the original PC metadata and early startup records.
This is a logging regression smoke check, not a new campaign validation.

The complete ARM64 syntax audit now reports **387/424 passing**, compared with
335/422 before the split. Two new shared/backend source units account for the
changed denominator. All 52 first errors caused by Quill's `mmap` dependency
are gone: 50 of those files now parse; two reveal separate crash-platform
dependencies. The final early-crash backend edit also passed a separate ARM64
recheck and the final NRO compile/link.

| Family | Passing | Audited |
| --- | ---: | ---: |
| Recovered game | 117 | 149 |
| Native overrides/platform | 223 | 227 |
| Archive bridge consumers | 3 | 3 |
| Shared host | 43 | 44 |
| Game entry point | 1 | 1 |

The 37 remaining first errors are 32 GCC linkage conflicts, two `sys/mman.h`
dependencies (arena memory and crash handling), two `dlfcn.h` dependencies
(crash reporting/symbols), and the cooperative scheduler's `ucontext.h`.
Syntax success does not establish target library availability or full-engine
linking.

Local evidence:

- `.kazarov/logging-test-results.log`, plus `port/build/logging-tests/Testing`.
- `.kazarov/pc-logging-build.log` and `.kazarov/pc-logging-smoke`.
- `port/build/switch-audit-logging/report.json` and per-source diagnostics;
  `switch-audit-logging-final-backend/report.json` records the final backend recheck.
- `port/build/switch/logging-probe/build.json`, `symbols.txt` and `probe.map`.

## Build the diagnostic

Use the prepared PC dependency/generated-source tree and native Windows Python
and CMake. The existing SDK is `C:/devkitPro`.

```powershell
cmake -S . -B port/build/switch -G Ninja -DPLATFORM=SWITCH
cmake --build port/build/switch --target switch_logging_probe
```

The output is
`port/build/switch/logging-probe/chronicletwo-logging-probe.nro`.
The direct alternative is:

```powershell
python scripts/switch/build_core_probe.py --logging --output port/build/switch/logging-probe
```

The NRO retains the math checks and uses the actual logging, shared policy,
printf, Kazarov sink and settings sources. The build verifies linked symbols
for its logging routines, native `pthread_create` and `fsdevMountSdmc`.
`build.json` records commands, source hashes, compiler, NRO hash and the
explicitly false hardware-validation flag. It includes no retail assets.

## Hardware test

1. Copy the NRO to
   `sd:/switch/ChronicleTwo-PC-Switch/chronicletwo-logging-probe.nro`.
2. Launch **ChronicleTwo PC-to-Switch logging probe** from Homebrew Menu.
3. Expect seven logging rows marked PASS, `Logging failure bits: 0x00`,
   the math checks marked PASS, and zero failed math comparisons.
4. Press **+** and verify that it returns to Homebrew Menu.
5. Inspect the new file under
   `sd:/switch/ChronicleTwo-PC-Switch/logging-probe/logs/darkchronicle-*.log`.
   It should contain `before file 42`, `probe session`, `worker 1 record 15`,
   `last partial`, and `late record`. Filtered `hidden` messages must be absent.

If a row fails, record the failure bits and that log. Bit values are 1 for
filtering/formatting, 2 for precedence, 4 for rate limits, 8 for bridges,
16 for worker delivery, 32 for shutdown/flush, and 64 for persisted readback.
This probe writes only its own diagnostic log directory; it does not load or
modify game saves or disc data. Existing probe session logs use normal retention.

## Run focused host checks

After the PC build has acquired the pinned compiler, Quill and GoogleTest sources:

```powershell
$llvm = "$PWD/.kazarov/windows/deps/llvm/llvm-mingw-20260922-ucrt-x86_64/bin"
$env:PATH = "$llvm;$env:PATH"
cmake -S scripts/switch/logging_tests -B port/build/logging-tests -G Ninja `
  -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_COMPILER=$llvm/clang++.exe" `
  "-DCMAKE_MAKE_PROGRAM=$PWD/.kazarov/windows/deps/ninja/ninja.exe"
cmake --build port/build/logging-tests -j4
ctest --test-dir port/build/logging-tests --output-on-failure -j4
```

The next platform work is the arena memory and cooperative context backend.
