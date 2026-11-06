"""Stage dependency notices from pinned sources, including on a clean checkout."""
import hashlib
import json
import shutil

from fetch_dependencies import ARTIFACTS


def stage_notices(root, stage):
    pc = root / "port/build/windows-x64-release/_deps"
    deps = root / ".kazarov/switch/deps"
    sources = {
        "ChronicleTwo-SDL3-LICENSE.txt": deps / "SDL-release-3.4.18/LICENSE.txt",
        "ChronicleTwo-luau-LICENSE.txt": pc / "luau-src/LICENSE.txt",
        "ChronicleTwo-fastgltf-LICENSE.md": pc / "fastgltf-src/LICENSE.md",
        "ChronicleTwo-simdjson-LICENSE": pc / "simdjson-src/LICENSE",
        "ChronicleTwo-simdjson-LICENSE-MIT": pc / "simdjson-src/LICENSE-MIT",
        "ChronicleTwo-NVK-NOTICE.txt": deps / "nvk-25.0.7/NOTICE.md",
    }
    for source in (root / "port/switch/licenses").glob("ChronicleTwo-*.txt"):
        sources[source.name] = source
    stage.mkdir(parents=True, exist_ok=True)
    records = []
    for name, source in sources.items():
        if not source.is_file():
            raise RuntimeError(f"Missing dependency notice: {source}")
        shutil.copy2(source, stage / name)
        records.append({"file": name, "source": source.relative_to(root).as_posix(),
                        "sha256": hashlib.sha256(source.read_bytes()).hexdigest()})
    manifest = {"packages": [{"name": name, "url": url, "sha256": digest}
                              for name, url, digest, folder in ARTIFACTS],
                "notices": records}
    (stage / "ChronicleTwo-dependency-sources.json").write_text(json.dumps(manifest, indent=2))
