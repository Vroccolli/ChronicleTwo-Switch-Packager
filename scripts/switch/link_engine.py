"""Compile the audited engine units and attempt a real Switch link; report gaps."""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from audit_compile import ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "port/build/switch-engine")
    parser.add_argument("--pc-build", type=Path, default=ROOT / "port/build/windows-x64-release")
    parser.add_argument("--devkitpro", type=Path, default=Path("C:/devkitPro"))
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--reuse-objects", action="store_true", help="reuse the existing audit report only if source hashes match")
    parser.add_argument("--libraries", type=Path, help="manifest of actual Switch static archives to link")
    parser.add_argument("--media-headers", type=Path, help="matching Switch FFmpeg include root")
    args = parser.parse_args()
    out, sdk = args.output.resolve(), args.devkitpro.resolve()
    out.mkdir(parents=True, exist_ok=True)
    audit = out / "objects"
    if not args.reuse_objects:
        audit_command = [sys.executable, str(ROOT / "scripts/switch/audit_compile.py"),
            "--services", "--objects", "--jobs", str(args.jobs), "--output", str(audit),
            "--pc-build", str(args.pc_build), "--devkitpro", str(sdk)]
        if args.media_headers: audit_command += ["--media-headers", str(args.media_headers)]
        subprocess.run(audit_command, check=False)
    report = json.loads((audit / "report.json").read_text())
    if report["syntax_only"] or not report["controlled_crash_adapter"]:
        raise RuntimeError("A services object audit is required")
    if args.media_headers and report.get("media_headers") != str(args.media_headers.resolve()):
        raise RuntimeError("Engine objects do not use the requested Switch FFmpeg headers")
    objects = []
    for entry in report["results"]:
        source = ROOT / entry["source"]
        if hashlib.sha256(source.read_bytes()).hexdigest() != entry["source_sha256"]:
            raise RuntimeError(f"Stale source: {source}")
        if entry["exit_code"] == 0:
            obj = audit / entry["log"].replace(".log", ".o")
            if not obj.exists():
                raise RuntimeError(f"Missing object: {obj}")
            objects.append(obj)
    suffix = ".exe" if os.name == "nt" else ""
    cc = sdk / "devkitA64/bin" / ("aarch64-none-elf-gcc" + suffix)
    cxx = sdk / "devkitA64/bin" / ("aarch64-none-elf-g++" + suffix)
    arch = ["-march=armv8-a+crc+crypto", "-mtune=cortex-a57", "-mtp=soft", "-fPIE"]
    env = dict(os.environ, DEVKITPRO=sdk.as_posix())
    commands, extra_sources = [], []
    for path in ["port/switch/runtime/platform_memory.c", "port/switch/runtime/fiber_switch.S",
                 "port/switch/crash/platform.c", "port/switch/platform/bootstrap.c",
                 "port/switch/platform/asset_verify.c"]:
        source = ROOT / path
        obj = out / (source.stem + ".o")
        command = [str(cc), *arch, "-funwind-tables", "-D__SWITCH__", "-I" + str(sdk / "libnx/include"),
            "-I" + str(ROOT / "port/switch"), "-c", str(source), "-o", str(obj)]
        commands.append(command)
        subprocess.run(command, env=env, check=True)
        objects.append(obj)
        extra_sources.append(dict(path=path, sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
    response = out / "objects.rsp"
    response.write_text("\n".join('"' + obj.as_posix() + '"' for obj in objects))
    elf = out / "chronicletwo-link-attempt.elf"
    elf.unlink(missing_ok=True)
    command = [str(cxx), *arch, "-specs=" + str(sdk / "libnx/switch.specs"),
        "-L" + str(sdk / "libnx/lib"), "-Wl,--gc-sections", "-Wl,-Map," + str(out / "engine.map"),
        "-o", str(elf), "@" + str(response), "-lnx", "-lm"]
    commands.append(command)
    libraries = []
    if args.libraries:
        manifest = json.loads(args.libraries.read_text())
        if manifest.get("platform") != "NintendoSwitch":
            raise RuntimeError("Refusing a library manifest without a NintendoSwitch target")
        libraries = [Path(path).resolve() for path in manifest["archives"]]
        for library in libraries:
            if not library.is_file(): raise RuntimeError(f"Missing library: {library}")
            headers = subprocess.check_output([str(sdk / "devkitA64/bin" / ("aarch64-none-elf-readelf" + suffix)), "-h", str(library)], text=True)
            machines = re.findall(r"Machine:\s*([^\r\n]+)", headers)
            if not machines or any(machine.strip() != "AArch64" for machine in machines):
                raise RuntimeError(f"Refusing non-AArch64 library: {library}")
        command[-2:-2] = ["-Wl,--start-group", *map(str, libraries), "-Wl,--end-group"]
    process = subprocess.run(command, env=env, capture_output=True, text=True, errors="replace", timeout=180)
    diagnostic = process.stdout + process.stderr
    (out / "link.log").write_text(diagnostic)
    missing = sorted(set(re.findall(r"undefined reference to [`']([^'\n]+)'", diagnostic)))
    duplicates = sorted(set(re.findall(r"multiple definition of [`']([^'\n]+)'", diagnostic)))
    def family(symbol):
        for prefix, name in [("SDL_", "SDL3"), ("vk", "Vulkan"), ("av", "FFmpeg"),
                             ("sws_", "FFmpeg"), ("swr_", "FFmpeg"), ("lua", "Luau")]:
            if symbol.startswith(prefix):
                return name
        if "cxxbridge" in symbol or "rust" in symbol: return "Rust/CXX bridge"
        if "cpptrace" in symbol: return "cpptrace"
        return "other (inspect exact symbols)"
    result = dict(all_source_objects=report["passed"] == report["total"],
        compiled=report["passed"], total=report["total"], linked=process.returncode == 0,
        link_exit_code=process.returncode, undefined_symbols=missing,
        multiple_definitions=duplicates,
        undefined_families=dict(Counter(family(symbol) for symbol in missing)),
        objects=[dict(path=str(obj), sha256=hashlib.sha256(obj.read_bytes()).hexdigest()) for obj in objects],
        libraries=[dict(path=str(lib), sha256=hashlib.sha256(lib.read_bytes()).hexdigest()) for lib in libraries],
        extra_sources=extra_sources, commands=commands,
        note="Real link using engine objects, libnx and verified AArch64 dependencies; desktop third-party binaries are never linked.")
    (out / "link-report.json").write_text(json.dumps(result, indent=2))
    print(json.dumps({key: result[key] for key in ("compiled", "total", "linked", "undefined_families")}, indent=2))
    if process.returncode:
        elf.unlink(missing_ok=True)
    return 0 if result["linked"] and result["all_source_objects"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
