"""Package the successfully linked native engine; never package a partial link."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[2]


def main():
    sdk=Path("C:/devkitPro")
    source=ROOT/"port/build/switch-engine"
    report=json.loads((source/"link-report.json").read_text())
    if not report["linked"] or not report["all_source_objects"] or report["undefined_symbols"] or report["multiple_definitions"]:
        raise RuntimeError("A complete successful engine link is required")
    for item in report["objects"]+report["libraries"]:
        if hashlib.sha256(Path(item["path"]).read_bytes()).hexdigest()!=item["sha256"]:
            raise RuntimeError("Stale link input: "+item["path"])
    out=ROOT/"port/build/switch/game"
    out.mkdir(parents=True,exist_ok=True)
    elf=out/"chronicletwo-switch.elf"; nacp=out/"game.nacp"; nro=out/"chronicletwo-switch.nro"
    shutil.copy2(source/"chronicletwo-link-attempt.elf",elf)
    commands=[]; env=dict(os.environ,DEVKITPRO=sdk.as_posix())
    def run(command):
        commands.append([str(x) for x in command]);subprocess.run(commands[-1],env=env,check=True)
    run([sdk/"tools/bin/nacptool.exe","--create","ChronicleTwo native Switch preview","ChronicleTwo contributors","0.7.0",nacp])
    run([sdk/"tools/bin/elf2nro.exe",elf,nro,"--nacp="+str(nacp),"--icon="+str(sdk/"libnx/default_icon.jpg")])
    symbols=subprocess.check_output([str(sdk/"devkitA64/bin/aarch64-none-elf-nm.exe"),"-C","--defined-only",str(elf)],text=True)
    for name in [" main","MainLoop(","gfx::RendererInit(","ctSwitchBootstrap","ctFiberSwap","__assert_func","ct::archive::open_archive("]:
        if name not in symbols: raise RuntimeError("Required game symbol missing: "+name)
    (out/"symbols.txt").write_text(symbols)
    shutil.copy2(source/"link-report.json",out/"link-report.json")
    stage=out/"app-deploy"
    stage.mkdir(exist_ok=True)
    shutil.copy2(nro,stage/nro.name)
    # Match the PC install: controller/keyboard glyphs live beside the binary.
    shutil.copytree(ROOT/"port/assets/glyphs",stage/"glyphs",dirs_exist_ok=True)
    runtime_assets=[dict(path=p.relative_to(stage).as_posix(),bytes=p.stat().st_size,
        sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted((stage/"glyphs").rglob("*")) if p.is_file()]
    manifest=dict(version="0.7.0",engine_linked=True,full_engine=True,hardware_tested=False,
        compiled=report["compiled"],total=report["total"],commands=commands,
        nro_sha256=hashlib.sha256(nro.read_bytes()).hexdigest(),elf_sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),
        default_run_root="/switch/ChronicleTwo-PC-Switch/game",default_device="sdmc",first_boot_graphics_thread="off",
        runtime_assets=runtime_assets)
    (out/"build.json").write_text(json.dumps(manifest,indent=2))
    print(f"Built {nro}: {nro.stat().st_size} bytes; game execution not yet verified")


if __name__=='__main__':main()
