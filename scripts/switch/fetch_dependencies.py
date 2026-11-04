"""Fetch pinned public port sources/packages into the ignored workspace cache."""
from pathlib import Path, PurePosixPath
import hashlib
import json
import os
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
ARTIFACTS = [
    ("SDL-release-3.4.18.tar.gz", "https://codeload.github.com/libsdl-org/SDL/tar.gz/refs/tags/release-3.4.18",
     "c4b08b950bd29d83caae0ab8d884298d27046a9d30700fb42841c50992665c90", "."),
    ("sdl3-switch-182e511214d7600e4bdab8606d7caf0ef744afd6.tar.gz",
     "https://codeload.github.com/neomody77/sdl3-switch/tar.gz/182e511214d7600e4bdab8606d7caf0ef744afd6",
     "8097d468dea2b4768272c81d1e175ff7b483c0ffb142b9b22917fa0e7a87fb71", "."),
    ("nvk-switch-25.0.7.tar.xz", "https://github.com/jhuz0/nvk-switch/releases/download/nvk-switch-mesa-25.0.7-r1/nvk-switch-25.0.7.tar.xz",
     "4ab244f982dc078aaff8c58640d7cf8a0f3ab43bbf2daadfd00c02fbf352af86", "nvk-25.0.7"),
    ("switch-ffmpeg-7.1-5-any.pkg.tar.zst", "https://pkg.devkitpro.org/packages/switch-ffmpeg-7.1-5-any.pkg.tar.zst",
     "22810bc38de15a1daed16496bfc6c9efcd2a1804f1409f43eaa2a7b4658a031c", "ffmpeg-7.1-5"),
    ("switch-dav1d-1.5.0-1-any.pkg.tar.zst", "https://pkg.devkitpro.org/packages/switch-dav1d-1.5.0-1-any.pkg.tar.zst",
     "612fa74cce39b1b7838d9d9b42bb4d1073fa8ccb6e4bbf092425190fa7826884", "dav1d-1.5.0-1"),
]


def main():
    cache = ROOT / ".kazarov/switch/deps"
    cache.mkdir(parents=True, exist_ok=True)
    records = []
    for name, url, digest, folder in ARTIFACTS:
        archive = cache / name
        if not archive.exists():
            if "pkg.devkitpro.org" in url:
                curl = "C:/Windows/System32/curl.exe" if os.name == "nt" else "curl"
                subprocess.run([curl,"--fail","--location","--noproxy","*","--connect-timeout","15",
                    "--max-time","45","-A","pacman/7.0.0","-o",str(archive),url],check=True)
            else:
                with urllib.request.urlopen(url, timeout=30) as response, archive.open("wb") as out:
                    while block := response.read(1024*1024): out.write(block)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Dependency checksum mismatch: {archive}")
        destination = (cache / folder).resolve()
        # Preserve already prepared source trees; clean extraction can be obtained
        # by selecting a fresh workspace, without changing the installed SDK.
        marker = cache / (name + ".extracted")
        if not marker.exists():
            destination.mkdir(parents=True, exist_ok=True)
            if name.endswith(".zst"):
                tar = "C:/Windows/System32/tar.exe" if os.name == "nt" else "tar"
                names = subprocess.check_output([tar,"-tf",str(archive)],text=True).splitlines()
                if any(PurePosixPath(n).is_absolute() or ".." in PurePosixPath(n).parts for n in names):
                    raise RuntimeError("Unsafe package member path")
                subprocess.run([tar,"-xf",str(archive),"-C",str(destination)],check=True)
            else:
                # Do not overwrite an already patched SDL source directory.
                if folder == "." and name.startswith("SDL-") and (cache/"SDL-release-3.4.18/CMakeLists.txt").exists():
                    pass
                else:
                    with tarfile.open(archive) as source: source.extractall(destination,filter="data")
            marker.write_text(digest + "\n")
        records.append(dict(name=name,url=url,sha256=digest,destination=str(destination)))
        print(name + ": verified")
    (cache/"provenance.json").write_text(json.dumps(records,indent=2))


if __name__ == "__main__":
    main()
