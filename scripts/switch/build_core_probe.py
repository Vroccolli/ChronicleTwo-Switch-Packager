"""Build a bounded Switch diagnostic using the real PC-generated game math units."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

from audit_compile import ROOT, preprocessor_options, split_command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pc-build", type=Path, default=ROOT / "port/build/windows-x64-release")
    parser.add_argument("--devkitpro", type=Path, default=Path("C:/devkitPro"))
    parser.add_argument("--output", type=Path, default=ROOT / "port/build/switch-core-probe")
    args = parser.parse_args()
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
    entries = json.loads((pc / "compile_commands.json").read_text())
    wanted = [pc / "ps2_src/mg_math.cpp", ROOT / "port/src/mg_math.cpp",
              ROOT / "tools/kazarov/src/cpp/sce/libvu0.cpp"]
    objects = []
    options = None
    for index, source in enumerate(wanted):
        entry = next(e for e in entries if Path(e["file"]).resolve() == source.resolve())
        arguments = entry.get("arguments") or split_command(entry["command"])
        opts = preprocessor_options(arguments)
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
    elf = output / "chronicletwo-core-probe.elf"
    nro = output / "chronicletwo-core-probe.nro"
    run([cxx, *arch, "-specs=" + str(sdk / "libnx/switch.specs"),
         "-L" + str(sdk / "libnx/lib"), "-Wl,--gc-sections", "-Wl,-Map," + str(output / "probe.map"),
         "-o", elf, output / "main.o", output / "checks.o", *objects, "-lnx", "-lm"])
    nacp = output / "probe.nacp"
    run([sdk / "tools/bin" / ("nacptool" + suffix), "--create", "ChronicleTwo PC-to-Switch core probe",
         "ChronicleTwo contributors", "0.1.0", nacp])
    run([sdk / "tools/bin" / ("elf2nro" + suffix), elf, nro,
         "--icon=" + str(sdk / "libnx/default_icon.jpg"), "--nacp=" + str(nacp)])
    nm = sdk / "devkitA64/bin" / ("aarch64-none-elf-nm" + suffix)
    symbols = subprocess.check_output([str(nm), "-C", "--defined-only", str(elf)], text=True)
    required = ["mgNormalizeVector(", "mgVectorInterpolate(", "sceVu0ApplyMatrix", "ctSwitchMathChecks"]
    for symbol in required:
        if symbol not in symbols:
            raise RuntimeError("Required linked symbol missing: " + symbol)
    (output / "symbols.txt").write_text(symbols)
    manifest = dict(sources=sources, commands=commands,
                    compiler=subprocess.check_output([str(cxx), "--version"], text=True).splitlines()[0],
                    pc_database_sha256=hashlib.sha256((pc / "compile_commands.json").read_bytes()).hexdigest(),
                    nro_sha256=hashlib.sha256(nro.read_bytes()).hexdigest(),
                    hardware_tested=False, full_engine=False, required_symbols=required)
    (output / "build.json").write_text(json.dumps(manifest, indent=2))
    print(f"Built {nro} ({nro.stat().st_size} bytes); hardware execution not yet tested")


if __name__ == "__main__":
    main()
