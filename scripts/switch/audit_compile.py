"""Compile-audit the PC target's real source list with devkitA64; never link or deploy."""
from __future__ import annotations

import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
from prepare_runtime import prepare_scheduler
from prepare_sources import prepare_source
from prepare_gfx import prepare_memory


ROOT = Path(__file__).resolve().parents[2]
FAMILIES = ("ct_ps2", "ct_port", "ct_port_bridge", "ct_host", "darkchronicle")


def split_command(command):
    if os.name != "nt":
        return shlex.split(command)
    count = ctypes.c_int()
    split = ctypes.windll.shell32.CommandLineToArgvW
    split.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
    split.restype = ctypes.POINTER(ctypes.c_wchar_p)
    pointer = split(command, ctypes.byref(count))
    if not pointer:
        raise OSError("CommandLineToArgvW failed")
    try:
        return [pointer[i] for i in range(count.value)]
    finally:
        ctypes.windll.kernel32.LocalFree(ctypes.cast(pointer, ctypes.c_void_p))


def preprocessor_options(arguments):
    result = []
    index = 1
    while index < len(arguments):
        arg = arguments[index]
        if arg in ("-I", "-isystem", "-include", "-D", "-U"):
            value = arguments[index + 1]
            if arg != "-D" or not re.match(r"(?:WINVER|_WIN32_WINNT|KZ_PORT_TRACY|CT_ENABLE_TRACY|TRACY_)", value):
                result.extend((arg, value))
            index += 2
            continue
        if arg.startswith(("-I", "-D", "-U")) and not re.match(
            r"-D(?:WINVER|_WIN32_WINNT|KZ_PORT_TRACY|CT_ENABLE_TRACY|TRACY_)", arg
        ):
            result.append(arg)
        index += 1
    return result


def diagnose(text):
    missing = re.search(r"fatal error: ([^\r\n]+): No such file or directory", text)
    if missing:
        return "missing header: " + missing[1]
    error = re.search(r"(?:fatal )?error: ([^\r\n]+)", text)
    return error[1] if error else "compiler failed (see log)"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pc-build", type=Path, default=ROOT / "port/build/windows-x64-release")
    parser.add_argument("--devkitpro", type=Path, default=Path("C:/devkitPro"))
    parser.add_argument("--output", type=Path, default=ROOT / "port/build/switch-audit")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--match", default="", help="optional source-path substring")
    parser.add_argument("--objects", action="store_true", help="emit ARM64 objects instead of only checking syntax")
    parser.add_argument("--runtime", action="store_true", help="select the native Switch memory and cooperative context adapters")
    parser.add_argument("--linkage", action="store_true", help="externalize only stub-exported names in generated GCC source copies")
    parser.add_argument("--services", action="store_true", help="select native runtime/data services, GCC linkage and controlled-crash adapters")
    parser.add_argument("--media-headers", type=Path, help="compile the movie backend against matching Switch FFmpeg headers")
    args = parser.parse_args()
    args.runtime = args.runtime or args.services
    args.linkage = args.linkage or args.services
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    sdk = args.devkitpro.resolve()
    compiler = sdk / "devkitA64/bin" / ("aarch64-none-elf-g++.exe" if os.name == "nt" else "aarch64-none-elf-g++")
    database = args.pc_build.resolve() / "compile_commands.json"
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    scheduler = prepare_scheduler(output / "generated") if args.runtime else None
    entries = json.loads(database.read_text())
    selected = []
    for entry in entries:
        arguments = entry.get("arguments") or split_command(entry["command"])
        command = " ".join(arguments).replace("\\", "/")
        family = next((name for name in FAMILIES if f"CMakeFiles/{name}.dir/" in command), None)
        source = Path(entry["file"]).resolve()
        if family and source.suffix == ".cpp" and args.match in str(source):
            selected.append((family, source, entry, arguments))
    fiber = ROOT / "port/switch/runtime/fiber.cpp"
    if args.runtime and args.match in str(fiber):
        selected.append(("switch_runtime", fiber, {"directory": str(ROOT)}, [str(compiler), "-I" + str(ROOT / "tools/kazarov/include")]))
    archive_source = ROOT / "port/switch/data/archive.cpp"
    if args.services and args.match in str(archive_source):
        selected.append(("switch_data", archive_source, {"directory": str(ROOT)}, [str(compiler)]))
    common_crash = ROOT / "port/src/crash/crash_common.cpp"
    if args.services and args.match in str(common_crash) and not any(item[1] == common_crash for item in selected):
        selected.append(("ct_port", common_crash, {"directory": str(ROOT)}, [str(compiler), "-I" + str(ROOT / "port/src")]))
    if not selected:
        parser.error("No matching PC game/host sources found in the compile database")
    version = subprocess.check_output([str(compiler), "--version"], text=True).splitlines()[0]
    common = [str(compiler), "-std=gnu++26", "-c" if args.objects else "-fsyntax-only", "-march=armv8-a+crc+crypto",
              "-mtp=soft", "-D__SWITCH__", "-D__NINTENDO_SWITCH__", "-DNDEBUG",
              "-fsigned-char", "-ffp-contract=off", "-fno-strict-aliasing", "-fwrapv", "-fmax-errors=3", "-w",
              "-I" + str(sdk / "libnx/include"), "-I" + str(sdk / "portlibs/switch/include")]
    if args.objects:
        common += ["-O2", "-mtune=cortex-a57", "-fPIE", "-ffunction-sections", "-fdata-sections", "-funwind-tables"]
    if args.runtime:
        common += ["-I" + str(ROOT / "port/switch")]

    def check(item):
        family, source, entry, arguments = item
        original = source
        if args.linkage and source.parent == args.pc_build.resolve() / "ps2_src":
            source = prepare_source(source, output / "generated/game")
        if args.runtime and source == ROOT / "tools/kazarov/src/cpp/platform/memory.cpp":
            source = ROOT / "port/switch/runtime/memory.cpp"
        elif args.runtime and source == ROOT / "tools/kazarov/src/cpp/sce/eekernel_threads.cpp":
            source = scheduler
        if args.services:
            if source == ROOT / "tools/kazarov/src/cpp/platform/files.cpp":
                source = ROOT / "port/switch/data/files.cpp"
            if source == ROOT / "tools/kazarov/src/cpp/gfx/memory.cpp":
                source = prepare_memory(output / "generated/gfx")
            crash_adapters = {"crash.cpp": "crash.cpp", "crash_report.cpp": "report.cpp", "crash_symbols.cpp": "symbols.cpp"}
            if source.parent == ROOT / "port/src/crash" and source.name in crash_adapters:
                source = ROOT / "port/switch/crash" / crash_adapters[source.name]
            if source == ROOT / "port/src/platform/legacy_encoding.cpp":
                source = ROOT / "port/switch/data/encoding.cpp"
        relative = source.relative_to(ROOT).as_posix()
        name = hashlib.sha256(relative.encode()).hexdigest()[:12] + "-" + source.stem + ".log"
        options = preprocessor_options(arguments)
        if args.media_headers:
            kept, index = [], 0
            while index < len(options):
                option = options[index]
                if option in ("-I", "-isystem") and "/.kazarov/windows/deps/ffmpeg/" in options[index+1].replace("\\", "/"):
                    index += 2
                    continue
                if option.startswith("-I") and "/.kazarov/windows/deps/ffmpeg/" in option.replace("\\", "/"):
                    index += 1
                    continue
                kept.append(option); index += 1
            options = ["-I" + str(args.media_headers.resolve())] + kept
        command = common + options + [str(source)]
        if args.objects:
            command.extend(("-o", str(output / name.replace(".log", ".o"))))
        try:
            process = subprocess.run(command, cwd=entry["directory"], capture_output=True,
                                     text=True, errors="replace", timeout=90)
            code, diagnostic = process.returncode, process.stdout + process.stderr
        except subprocess.TimeoutExpired:
            code, diagnostic = 124, "compiler timeout after 90 seconds"
        (output / name).write_text(diagnostic, encoding="utf-8")
        dependencies=[]
        if original == ROOT / "port/src/present.cpp":
            dependency = ROOT / "port/src/present_timing.hpp"
            dependencies.append(dict(path=dependency.relative_to(ROOT).as_posix(),
                sha256=hashlib.sha256(dependency.read_bytes()).hexdigest()))
        if args.services and original == ROOT / "tools/kazarov/src/cpp/gfx/memory.cpp":
            for dependency in (original, ROOT / "port/switch/gfx/memory_policy.hpp"):
                dependencies.append(dict(path=dependency.relative_to(ROOT).as_posix(),
                    sha256=hashlib.sha256(dependency.read_bytes()).hexdigest()))
        if args.services and original in (ROOT / "tools/kazarov/src/cpp/platform/files.cpp",
                ROOT / "port/src/platform/config.cpp", ROOT / "port/src/save_slots.cpp"):
            for dependency in (ROOT / "port/switch/data/files.hpp", ROOT / "tools/kazarov/include/platform/files.hpp"):
                dependencies.append(dict(path=dependency.relative_to(ROOT).as_posix(),
                    sha256=hashlib.sha256(dependency.read_bytes()).hexdigest()))
        return dict(family=family, source=relative, source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                    dependencies=dependencies,
                    original_source=original.relative_to(ROOT).as_posix(), runtime_adapter=source != original,
                    exit_code=code, blocker=None if code == 0 else diagnose(diagnostic), log=name, command=command)

    print(f"Switch {'object' if args.objects else 'syntax'} audit: {len(selected)} PC units, {args.jobs} workers", flush=True)
    results = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for result in pool.map(check, selected):
            results.append(result)
            if len(results) % 25 == 0 or len(results) == len(selected):
                print(f"  {len(results)}/{len(selected)} checked; {sum(r['exit_code'] == 0 for r in results)} passed", flush=True)
    report = dict(compiler=version, pc_database=str(database), syntax_only=not args.objects,
                  optimization="-O2" if args.objects else None,
                  cpu_tuning="cortex-a57" if args.objects else None,
                  runtime_adapters=args.runtime,
                  linkage_adapter=args.linkage,
                  controlled_crash_adapter=args.services,
                  native_data_adapter=args.services,
                  media_headers=str(args.media_headers.resolve()) if args.media_headers else None,
                  note="PC-generated sources and header-only dependencies reused; no ARM libraries, link, NRO or hardware execution established.",
                  total=len(results), passed=sum(r["exit_code"] == 0 for r in results), results=results)
    (output / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    blockers = Counter(r["blocker"] for r in results if r["exit_code"])
    for reason, count in blockers.most_common(12):
        print(f"  {count}: {reason}", flush=True)
    print(f"Report: {output / 'report.json'}", flush=True)
    return 1 if blockers else 0


if __name__ == "__main__":
    raise SystemExit(main())
