"""Build a bounded Switch diagnostic using the real PC-generated game math units."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

from audit_compile import ROOT, preprocessor_options, split_command
from prepare_runtime import prepare_scheduler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pc-build", type=Path, default=ROOT / "port/build/windows-x64-release")
    parser.add_argument("--devkitpro", type=Path, default=Path("C:/devkitPro"))
    parser.add_argument("--output", type=Path, default=ROOT / "port/build/switch-core-probe")
    parser.add_argument("--logging", action="store_true", help="include the real logging backend and SD file checks")
    parser.add_argument("--runtime", action="store_true", help="include native arena memory and the shared cooperative scheduler")
    parser.add_argument("--services", action="store_true", help="include controlled-crash report persistence and raw address checks")
    args = parser.parse_args()
    args.runtime = args.runtime or args.services
    args.logging = args.logging or args.runtime
    sdk, pc, output = args.devkitpro.resolve(), args.pc_build.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    suffix = ".exe" if os.name == "nt" else ""
    cxx = sdk / "devkitA64/bin" / ("aarch64-none-elf-g++" + suffix)
    cc = sdk / "devkitA64/bin" / ("aarch64-none-elf-gcc" + suffix)
    env = dict(os.environ, DEVKITPRO=sdk.as_posix())
    commands, sources = [], []

    def run(command):
        commands.append([str(v) for v in command])
        subprocess.run(commands[-1], cwd=ROOT, env=env, check=True)

    arch = ["-march=armv8-a+crc+crypto", "-mtune=cortex-a57", "-mtp=soft", "-fPIE"]
    flags = arch + ["-O2", "-ffunction-sections", "-fdata-sections", "-ffp-contract=off", "-fsigned-char",
                    "-D__SWITCH__", "-DNDEBUG", "-I" + str(sdk / "libnx/include")]
    if args.logging:
        flags.append("-DCT_SWITCH_LOG_PROBE")
    if args.runtime:
        flags += ["-DCT_SWITCH_RUNTIME_PROBE", "-I" + str(ROOT / "port/switch")]
    if args.services:
        flags.append("-DCT_SWITCH_SERVICES_PROBE")
    entries = json.loads((pc / "compile_commands.json").read_text())
    wanted = [pc / "ps2_src/mg_math.cpp", ROOT / "port/src/mg_math.cpp",
              ROOT / "tools/kazarov/src/cpp/sce/libvu0.cpp"]
    original_scheduler = ROOT / "tools/kazarov/src/cpp/sce/eekernel_threads.cpp"
    scheduler = prepare_scheduler(output / "generated") if args.runtime else None
    if args.runtime:
        wanted += [ROOT / "tools/kazarov/src/cpp/platform/arena.cpp",
                   ROOT / "tools/kazarov/src/cpp/platform/clock.cpp", original_scheduler,
                   ROOT / "tools/kazarov/src/cpp/sce/unimplemented.cpp"]
    objects = []
    options = None
    for index, source in enumerate(wanted):
        entry = next(e for e in entries if Path(e["file"]).resolve() == source.resolve())
        arguments = entry.get("arguments") or split_command(entry["command"])
        opts = preprocessor_options(arguments)
        if source == original_scheduler:
            source = scheduler
        obj = output / f"game-{index}.o"
        run([cxx, "-std=gnu++26", *flags, *opts, "-c", source, "-o", obj])
        objects.append(obj)
        sources.append(dict(path=source.relative_to(ROOT).as_posix(), sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
        if source == ROOT / "port/src/mg_math.cpp":
            options = opts
    checks = ROOT / "port/switch/core_probe/checks.cpp"
    driver = ROOT / "port/switch/core_probe/main.c"
    for source in (checks, driver):
        sources.append(dict(path=source.relative_to(ROOT).as_posix(), sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
    run([cxx, "-std=gnu++26", *flags, *options, "-c", checks, "-o", output / "checks.o"])
    run([cc, "-std=gnu11", *flags, "-c", driver, "-o", output / "main.o"])
    if args.logging:
        logging_sources = [ROOT / "port/switch/core_probe/log_checks.cpp",
                           ROOT / "port/src/log/log_sync.cpp", ROOT / "port/src/log/log_common.cpp",
                           ROOT / "port/src/log/ps2_printf.cpp", ROOT / "tools/kazarov/src/cpp/platform/log.cpp",
                           ROOT / "tools/kazarov/src/cpp/platform/settings.cpp"]
        includes = ["-I" + str(ROOT / directory) for directory in ("port/src", "port/include", "tools/kazarov/include")]
        for index, source in enumerate(logging_sources):
            obj = output / f"logging-{index}.o"
            run([cxx, "-std=gnu++26", *flags, *includes, "-c", source, "-o", obj])
            objects.append(obj)
            sources.append(dict(path=source.relative_to(ROOT).as_posix(), sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
    if args.runtime:
        runtime_sources = [ROOT / "port/switch/runtime/memory.cpp", ROOT / "port/switch/runtime/fiber.cpp",
                           ROOT / "port/switch/core_probe/runtime_checks.cpp"]
        includes = ["-I" + str(ROOT / directory) for directory in ("port/src", "port/include", "tools/kazarov/include", "port/switch")]
        includes += ["-I" + str(pc / "ps2_include/sce")]
        for index, source in enumerate(runtime_sources):
            obj = output / f"runtime-{index}.o"
            run([cxx, "-std=gnu++26", *flags, *includes, "-c", source, "-o", obj])
            objects.append(obj)
            sources.append(dict(path=source.relative_to(ROOT).as_posix(), sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
        for source in [ROOT / "port/switch/runtime/platform_memory.c", ROOT / "port/switch/runtime/fiber_switch.S",
                       ROOT / "port/switch/runtime/fiber_abi_check.S"]:
            obj = output / (source.stem + ".o")
            run([cc, *flags, "-c", source, "-o", obj])
            objects.append(obj)
            sources.append(dict(path=source.relative_to(ROOT).as_posix(), sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
    if args.services:
        service_sources = [ROOT / path for path in (
            "port/switch/crash/crash.cpp", "port/switch/crash/report.cpp", "port/switch/crash/symbols.cpp",
            "port/src/crash/crash_common.cpp", "port/src/crash/crash_context.cpp",
            "port/switch/core_probe/services_checks.cpp", "port/switch/crash/platform.c")]
        for index, source in enumerate(service_sources):
            obj = output / f"services-{index}.o"
            compiler, standard = (cc, "-std=gnu11") if source.suffix == ".c" else (cxx, "-std=gnu++26")
            run([compiler, standard, *flags, *includes, "-c", source, "-o", obj])
            objects.append(obj)
            sources.append(dict(path=source.relative_to(ROOT).as_posix(), sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
    name = "chronicletwo-services-probe" if args.services else ("chronicletwo-runtime-probe" if args.runtime else ("chronicletwo-logging-probe" if args.logging else "chronicletwo-core-probe"))
    elf = output / (name + ".elf")
    nro = output / (name + ".nro")
    run([cxx, *arch, "-specs=" + str(sdk / "libnx/switch.specs"),
         "-L" + str(sdk / "libnx/lib"), "-Wl,--gc-sections", "-Wl,-Map," + str(output / "probe.map"),
         "-o", elf, output / "main.o", output / "checks.o", *objects, "-lnx", "-lm"])
    nacp = output / "probe.nacp"
    title = "ChronicleTwo PC-to-Switch services probe" if args.services else ("ChronicleTwo PC-to-Switch runtime probe" if args.runtime else ("ChronicleTwo PC-to-Switch logging probe" if args.logging else "ChronicleTwo PC-to-Switch core probe"))
    run([sdk / "tools/bin" / ("nacptool" + suffix), "--create", title,
         "ChronicleTwo contributors", "0.4.1" if args.services else ("0.3.0" if args.runtime else ("0.2.1" if args.logging else "0.1.0")), nacp])
    run([sdk / "tools/bin" / ("elf2nro" + suffix), elf, nro,
         "--icon=" + str(sdk / "libnx/default_icon.jpg"), "--nacp=" + str(nacp)])
    nm = sdk / "devkitA64/bin" / ("aarch64-none-elf-nm" + suffix)
    symbols = subprocess.check_output([str(nm), "-C", "--defined-only", str(elf)], text=True)
    required = ["mgNormalizeVector(", "mgVectorInterpolate(", "sceVu0ApplyMatrix", "ctSwitchMathChecks"]
    if args.logging:
        required += ["ctSwitchLogChecks", "LogAttachFile(", "LogFlushForCrash(", "log_detail::SyncWrite(",
                     "pthread_create", "fsdevMountSdmc"]
    if args.runtime:
        required += ["ctSwitchRuntimeChecks", "ctFiberSwap", "ctFiberAbiCheck", "ArenaMemoryMap(",
                     "WaitSema", "svcSetMemoryPermission", "svcQueryMemory"]
    if args.services:
        required += ["ctSwitchServicesChecks", "crash_detail::WriteBundle(", "ctCrashFrames", "CrashReportAndExit("]
    for symbol in required:
        if symbol not in symbols:
            raise RuntimeError("Required linked symbol missing: " + symbol)
    (output / "symbols.txt").write_text(symbols)
    manifest = dict(sources=sources, commands=commands,
                    compiler=subprocess.check_output([str(cxx), "--version"], text=True).splitlines()[0],
                    pc_database_sha256=hashlib.sha256((pc / "compile_commands.json").read_bytes()).hexdigest(),
                    nro_sha256=hashlib.sha256(nro.read_bytes()).hexdigest(),
                    hardware_tested=False, full_engine=False, logging_backend="synchronous" if args.logging else None,
                    cooperative_runtime=args.runtime,
                    controlled_crash_services=args.services,
                    required_symbols=required)
    (output / "build.json").write_text(json.dumps(manifest, indent=2))
    print(f"Built {nro} ({nro.stat().st_size} bytes); hardware execution not yet tested")


if __name__ == "__main__":
    main()
