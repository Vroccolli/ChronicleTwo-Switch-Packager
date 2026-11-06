"""Build the native PC generation tools, complete Switch engine, and optional Windows packager."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from toolchain import sdk_root, cmake_path, git_path, find_tool

ROOT = Path(__file__).resolve().parents[2]
SDK_FILES = [
    "devkitA64/bin/aarch64-none-elf-g++.exe", "libnx/lib/libnx.a", "libnx/include/switch.h",
    "tools/bin/elf2nro.exe", "tools/bin/nacptool.exe",
    *["portlibs/switch/lib/" + name for name in
      ("libEGL.a", "libGLESv2.a", "libglapi.a", "libdrm_nouveau.a", "libz.a", "libzstd.a", "libbz2.a")],
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--devkitpro", type=Path, default=sdk_root())
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--packager", action="store_true", help="also freeze the offline Windows app")
    parser.add_argument("--check", action="store_true", help="check prerequisites without downloading or compiling")
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("The documented build currently requires native Windows x64")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    sdk = args.devkitpro.resolve()
    missing = [str(sdk / name) for name in SDK_FILES if not (sdk / name).is_file()]
    if missing:
        raise RuntimeError("Install the SDK packages documented in docs/BUILD_SWITCH.md. Missing:\n" + "\n".join(missing))
    cmake = cmake_path()
    git = git_path()
    pwsh = find_tool("pwsh", "CT_PWSH")
    find_tool("7z", "CT_7ZIP", (Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "7-Zip/7z.exe",))
    if not (ROOT / "tools/kazarov/CMakeLists.txt").is_file():
        raise RuntimeError("Initialize the pinned submodule: git submodule update --init --recursive")
    if sys.version_info < (3, 11):
        raise RuntimeError("Python 3.11 or newer is required")
    print(f"Prerequisites ready. SDK: {sdk}; CMake: {cmake}; jobs: {args.jobs}", flush=True)
    if args.check:
        return
    env = dict(os.environ, DEVKITPRO=sdk.as_posix(), CT_CMAKE=cmake, CT_GIT=git, JOBS=str(args.jobs))
    # Ensure the PC scripts choose this native Python/CMake/Git, ahead of MSYS.
    env["PATH"] = os.pathsep.join([str(Path(sys.executable).parent), str(Path(cmake).parent),
                                  str(Path(git).parent), str(Path(os.environ.get("SystemRoot", "C:/Windows")) / "System32"),
                                  env.get("PATH", "")])
    env["CT_PYTHON"] = sys.executable
    env["CT_7ZIP"] = find_tool("7z", "CT_7ZIP", (Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "7-Zip/7z.exe",))
    def run(command):
        subprocess.run([str(x) for x in command], cwd=ROOT, env=env, check=True)
    run([pwsh, "-NoProfile", "-File", ROOT / "tools/windows/kazarov.ps1", "build", "windows-x64-release"])
    run([sys.executable, ROOT / "scripts/switch/build_game.py", "--jobs", args.jobs,
         "--devkitpro", sdk, "--cmake", cmake])
    if args.packager:
        venv = ROOT / ".kazarov/packager-venv"
        if not (venv / "Scripts/python.exe").is_file():
            run([sys.executable, "-m", "venv", venv])
        python = venv / "Scripts/python.exe"
        run([python, "-m", "pip", "install", "-r", ROOT / "scripts/packager/requirements-build.txt"])
        run([python, ROOT / "scripts/packager/build_release.py"])
    report = json.loads((ROOT / "port/build/switch/game/build.json").read_text())
    print(f"Built engine {report['version']}: {ROOT / 'port/build/switch/game/chronicletwo-switch.nro'}", flush=True)


if __name__ == "__main__":
    main()
