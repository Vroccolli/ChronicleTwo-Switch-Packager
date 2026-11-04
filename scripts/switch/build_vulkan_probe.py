"""Check the existing engine's real requirements against the experimental Switch ICD."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
from audit_compile import ROOT


def main():
    sdk = Path("C:/devkitPro")
    driver = ROOT / ".kazarov/switch/deps/nvk-25.0.7"
    library = driver / "lib/libvulkan.a"
    if hashlib.sha256(library.read_bytes()).hexdigest() != "ff65a74b46fe749f6be73eac3c8968d70e312c7646f815f26b69f4aa35737d26":
        raise RuntimeError("Vulkan driver archive does not match the pinned package")
    out = ROOT / "port/build/switch/vulkan-probe"
    out.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, DEVKITPRO=sdk.as_posix())
    commands, sources, objects = [], [], []
    vulkan_headers = next((ROOT / ".kazarov/windows/deps/vulkan").glob("Vulkan-Headers-*/include"))
    common = ["-march=armv8-a+crc+crypto", "-mtune=cortex-a57", "-mtp=soft", "-fPIE",
        "-ffunction-sections", "-fdata-sections", "-fsigned-char", "-ffp-contract=off", "-D__SWITCH__", "-DNDEBUG",
        "-I" + str(vulkan_headers), "-I" + str(driver / "include"), "-I" + str(sdk / "libnx/include"),
        "-I" + str(ROOT / "tools/kazarov/include")]
    def run(command):
        commands.append([str(x) for x in command])
        subprocess.run(commands[-1], env=env, cwd=ROOT, check=True)
    for source in [ROOT / "port/switch/vulkan_probe/main.c", ROOT / "port/switch/vulkan_probe/checks.cpp",
                   ROOT / "tools/kazarov/src/cpp/gfx/requirements.cpp"]:
        compiler = sdk / "devkitA64/bin" / ("aarch64-none-elf-gcc.exe" if source.suffix == ".c" else "aarch64-none-elf-g++.exe")
        obj = out / (source.stem + ".o")
        run([compiler, "-std=gnu11" if source.suffix == ".c" else "-std=gnu++26", "-O2", *common, "-c", source, "-o", obj])
        objects.append(obj)
        sources.append(dict(path=source.relative_to(ROOT).as_posix(), sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
    elf = out / "chronicletwo-vulkan-probe.elf"
    run([sdk / "devkitA64/bin/aarch64-none-elf-g++.exe", *common,
        "-specs=" + str(sdk / "libnx/switch.specs"), "-L" + str(sdk / "libnx/lib"),
        "-L" + str(sdk / "portlibs/switch/lib"), "-Wl,--gc-sections", "-Wl,-Map," + str(out / "probe.map"),
        "-o", elf, *objects, "-Wl,--start-group", library, "-lz", "-lzstd", "-lnx", "-lm", "-Wl,--end-group"])
    nacp = out / "probe.nacp"
    run([sdk / "tools/bin/nacptool.exe", "--create", "ChronicleTwo Switch Vulkan capability probe", "ChronicleTwo contributors", "0.5.0", nacp])
    nro = out / "chronicletwo-vulkan-probe.nro"
    run([sdk / "tools/bin/elf2nro.exe", elf, nro, "--nacp=" + str(nacp), "--icon=" + str(sdk / "libnx/default_icon.jpg")])
    (out / "build.json").write_text(json.dumps(dict(sources=sources, commands=commands,
        driver_archive_sha256=hashlib.sha256(library.read_bytes()).hexdigest(),
        nro_sha256=hashlib.sha256(nro.read_bytes()).hexdigest(), hardware_tested=False, full_engine=False), indent=2))
    print(f"Built {nro}: {nro.stat().st_size} bytes")


if __name__ == "__main__":
    main()
