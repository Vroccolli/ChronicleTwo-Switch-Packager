# Switch memory and cooperative contexts: 2026-11-04

The new adapters implement the existing Kazarov arena interface and supply an
ARM64 context backend for its existing PS2 cooperative scheduler. The runtime
diagnostic 0.3.0 builds and is uploaded. Hardware execution remains pending.

## Implementation

`port/switch/runtime/memory.cpp` allocates page-aligned, zeroed memory with a
kernel-protected trailing guard page. It checks allocation arithmetic, preserves
the arena's lead/capacity layout and restores permissions before freeing memory.
The small C bridge calls libnx without exposing its types to PS2 C++ headers.
Both zeroing operations preserve pointers and clear bytes in place. Physical
pages remain committed; this does not implement sparse reservation or decommit.
Outstanding mappings are tracked for final process cleanup. That cleanup is
terminal: arena owners must not be reused after their mappings are released.

The ARM64 context saves x19–x30, SP, full q8–q15, FPCR and FPSR. Stack pointers
stay 16-byte aligned, with guard pages at both ends of each fiber stack.
The register and stack rules follow
[Arm AAPCS64](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst).
Contexts run on the same native thread, retaining its platform/TLS state.

`scripts/switch/prepare_runtime.py` creates a build copy of Kazarov's scheduler,
replacing only platform headers and four context functions. It asserts that
the shared body beginning at `struct Thread` remains identical to upstream.
Priority, semaphore and scheduling logic are reused; the submodule is unchanged.

## Validation

- All 28 Debug host tests pass, including real guard faults, zeroing, arena
  reuse/retirement, overflow rejection, priority and semaphore behavior. These
  tests use the new arena adapter with a real Windows memory bridge and the
  original Windows context backend. They do not execute the ARM64 assembly.
- The adapted ARM64 syntax audit passes 390/425 units. It explicitly substitutes
  the memory and scheduler adapters and adds the fiber source. The 35 remaining
  first errors are 32 GCC linkage conflicts, two `dlfcn.h` consumers and one
  crash-handler `sys/mman.h` dependency. This is not a full engine link or a
  measure of game-port completion; renderer and third-party libraries remain open.
- The probe links real arena, scheduler, logging and game math code into an
  AArch64 ELF/NRO. Required symbols and source hashes were checked.

Evidence stays in ignored build output: `port/build/runtime-tests`,
`port/build/switch-audit-runtime/report.json` and
`port/build/switch/runtime-probe/build.json`.

## Build

With the prepared PC tree and native Windows CMake/Python available:

```powershell
cmake -S . -B port/build/switch -G Ninja -DPLATFORM=SWITCH
cmake --build port/build/switch --target switch_runtime_probe
python scripts/switch/audit_compile.py --runtime --jobs 4 --output port/build/switch-audit-runtime
```

For host regressions, configure `scripts/switch/runtime_tests` with Debug,
the pinned LLVM compiler and Ninja, then build and run CTest. It requires
the PC tree's acquired GoogleTest source. Set `Python3_EXECUTABLE` explicitly
when MSYS Python precedes native Python on PATH.

## Uploaded diagnostic and hardware test

The dedicated FTP deployment helper uploaded only
`/switch/ChronicleTwo-PC-Switch/chronicletwo-runtime-probe.nro`.
Downloaded readback matched 1,140,933 bytes and SHA-256
`5fef902c4544d1b4ea8126b63e5f2d641503f1f62142d3e50c263fc119be1757`.
The receipt is `port/build/switch/runtime-probe/upload-verification-0.3.0.json`.
The existing logging probe remains available.

1. Fully close the previous probe. Launch **ChronicleTwo PC-to-Switch runtime
   probe**, version 0.3.0, from Homebrew Menu.
2. Expect eight runtime rows marked PASS, `Runtime failure bits: 0x00`,
   `ABI errors=0 stack errors=0 FP errors=0`, all logging rows PASS with
   `Logging failure bits: 0x00`, and math PASS.
3. Take an Album screenshot. If it stops during execution, capture the last
   displayed stage marker. Press **+** after completion to exit.

The runtime rows cover arena layout/zeroing, kernel guard permissions, owner
reuse/retirement, 1,000-round cooperative FIFO stress, ARM64 registers/stack/FP,
priority/restart, semaphore wait/cancel and mapping cleanup. Failure bits are
1, 2, 4, 8, 16, 32, 64 and 128 respectively. Device guard checks query kernel
permissions; deliberate guard faults are tested on the host only.

The probe keeps live stack canaries and checks same-thread execution and
per-context floating-point rounding through repeated switches. Its assembly
checker verifies callee-saved registers after yielding. Logs are under
`sd:/switch/ChronicleTwo-PC-Switch/runtime-probe/logs`. The probe does not
load retail assets or game saves. Hardware results must be recorded separately
from successful build and transfer.
