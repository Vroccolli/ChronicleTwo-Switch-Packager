"""Developer-only: assemble an offline Windows package from checked native builds."""
from pathlib import Path
import argparse
import json
import shutil
import subprocess
import sys
import tempfile
import zipfile

from packager import digest, verify_payload

ROOT = Path(__file__).resolve().parents[2]


def build(build_root, output):
    report = json.loads((build_root / "build.json").read_text())
    if not report["engine_linked"] or not report["full_engine"] or report["compiled"] != report["total"]:
        raise RuntimeError("A complete engine build is required")
    nro = build_root / "chronicletwo-switch.nro"
    if digest(nro) != report["nro_sha256"]:
        raise RuntimeError("NRO does not match its build report")
    if report["default_run_root"] != "/switch/ChronicleTwo-PC-Switch/game":
        raise RuntimeError("Unexpected runtime installation path")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="bundle-", dir=output) as temporary:
        work = Path(temporary)
        payload = work / "payload"
        runtime, tools = payload / "runtime", payload / "tools"
        runtime.mkdir(parents=True)
        tools.mkdir()
        shutil.copy2(nro, runtime / nro.name)
        shutil.copytree(ROOT / "port/assets/glyphs", runtime / "glyphs")
        for asset in report["runtime_assets"]:
            if asset["path"].startswith("glyphs/") and digest(runtime / asset["path"]) != asset["sha256"]:
                raise RuntimeError("Glyph assets differ from the engine package")
        # Older build manifests omit notices from runtime_assets; retain the staged notices too.
        notices = list((build_root / "app-deploy").glob("ChronicleTwo-*"))
        if not any(p.name == "ChronicleTwo-dependency-sources.json" for p in notices):
            raise RuntimeError("The native engine package must include its dependency notices")
        for notice in notices:
            if notice.is_file():
                shutil.copy2(notice, runtime / notice.name)
        # Partial configuration is merged with engine defaults.
        (runtime / "game").mkdir()
        (runtime / "game/config.json").write_text('{"video":{"max_fps":30.0}}\n')
        native = ROOT / "port/build/windows-x64-release"
        shutil.copy2(native / "dc2data.exe", tools / "dc2data.exe")
        # The extractor needs the matching source-derived gameplay seed, not retail assets.
        seed = native / "assets/data.zip"
        with zipfile.ZipFile(seed) as archive:
            if not archive.namelist() or any(not name.startswith("gameplay/") or not name.endswith(".json") for name in archive.namelist()):
                raise RuntimeError("Expected a gameplay-only source seed, not a retail archive")
        (tools / "assets").mkdir()
        shutil.copy2(seed, tools / "assets/data.zip")
        licenses = payload / "licenses"
        licenses.mkdir()
        ffmpeg = ROOT / ".kazarov/windows/deps/ffmpeg/ffmpeg-n9.0.2-45-g1e65c6cd5d-win64-lgpl-shared-9.0"
        shutil.copy2(ffmpeg / "LICENSE.txt", licenses / "FFmpeg-LGPL.txt")
        llvm = ROOT / ".kazarov/windows/deps/llvm/llvm-mingw-20260922-ucrt-x86_64"
        shutil.copy2(llvm / "LICENSE.TXT", licenses / "LLVM-MinGW.txt")
        # Resolve all non-system DLLs transitively from the extractor's build directory.
        import pefile
        pending = [tools / "dc2data.exe"]
        while pending:
            current = pending.pop()
            pe = pefile.PE(str(current))
            names = [entry.dll.decode("ascii") for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", [])]
            pe.close()
            for name in names:
                dependency = native / name
                target = tools / name
                if dependency.is_file() and not target.exists():
                    shutil.copy2(dependency, target)
                    pending.append(target)
                elif not target.exists() and not name.lower().startswith(("api-ms-", "ext-ms-")):
                    windows = Path(__import__("os").environ.get("SystemRoot", "C:/Windows")) / "System32"
                    if not (windows / name).exists():
                        raise RuntimeError(f"Missing extractor dependency: {name}")
        manifest = {"schema": 1, "switch_version": report["version"],
                    "hardware_tested": report.get("hardware_tested", False),
                    "minimum_free_bytes": 12 * 1024**3,
                    "supported_iso_sha256": [(ROOT / "rom/pal/checksum.sha256").read_text().split()[0]],
                    "files": {p.relative_to(payload).as_posix(): {"bytes": p.stat().st_size, "sha256": digest(p)}
                              for p in payload.rglob("*") if p.is_file()}}
        (payload / "manifest.json").write_text(json.dumps(manifest, indent=2))
        verify_payload(payload)
        (output / "payload-manifest.json").write_text(json.dumps(manifest, indent=2))
        subprocess.run([sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean", "--onedir",
                        "--windowed", "--name", "ChronicleTwo-Switch-Packager",
                        "--distpath", str(output), "--workpath", str(work / "freeze"),
                        "--specpath", str(work), "--add-data", f"{payload};payload",
                        str(Path(__file__).with_name("packager.py"))], check=True)
    folder = output / "ChronicleTwo-Switch-Packager"
    shutil.copy2(Path(__file__).with_name("USER_README.txt"), folder / "READ ME.txt")
    shutil.make_archive(str(output / "ChronicleTwo-Switch-Packager"), "zip", folder)
    print(folder / "ChronicleTwo-Switch-Packager.exe")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=Path, default=ROOT / "port/build/switch/game")
    parser.add_argument("--output", type=Path, default=ROOT / "port/build/packager-release")
    args = parser.parse_args()
    build(args.build_root.resolve(), args.output.resolve())
