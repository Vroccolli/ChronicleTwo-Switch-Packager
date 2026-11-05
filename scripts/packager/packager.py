"""Offline end-user installer. Frozen with its verified runtime payload."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import queue
import shutil
import subprocess
import sys
import tempfile
import threading

APP = "ChronicleTwo-PC-Switch"


def digest(path, progress=None):
    h = hashlib.sha256()
    size = path.stat().st_size
    count = 0
    last_percent = -1
    with path.open("rb") as stream:
        while block := stream.read(4 * 1024 * 1024):
            h.update(block)
            count += len(block)
            percent = count * 100 // max(size, 1)
            if progress and percent != last_percent:
                progress(f"Checking disc: {percent}%")
                last_percent = percent
    return h.hexdigest()


def payload_root():
    return Path(getattr(sys, "_MEIPASS", Path(__file__).parent)) / "payload"


def verify_payload(payload):
    manifest = json.loads((payload / "manifest.json").read_text(encoding="utf-8"))
    for name, expected in manifest["files"].items():
        relative = Path(name)
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("Invalid bundle path")
        path = payload / relative
        if not path.is_file() or path.stat().st_size != expected["bytes"] or digest(path) != expected["sha256"]:
            raise ValueError(f"The packager is damaged: {name}. Extract a fresh copy.")
    required = {"tools/dc2data.exe", "tools/assets/data.zip", "runtime/chronicletwo-switch.nro", "runtime/glyphs/glyphs.json"}
    if not required.issubset(manifest["files"]):
        raise ValueError("Incomplete runtime bundle")
    return manifest


def run_tool(command, log, report):
    # No shell, inherited PATH, working-directory DLL lookup, or console popup.
    env = dict(os.environ)
    env["PATH"] = str(Path(command[0]).parent) + os.pathsep + str(Path(os.environ.get("SystemRoot", "C:/Windows")) / "System32")
    with subprocess.Popen([str(arg) for arg in command], cwd=Path(command[0]).parent,
                          env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0)) as process:
        for line in process.stdout:
            message = line.decode("utf-8", errors="replace").rstrip()
            log.write(message + "\n")
            log.flush()
            report(message)
        if process.wait():
            raise RuntimeError("Game-data preparation failed. See packager.log in the unfinished folder.")


def install(iso, output, payload, report=print, runner=run_tool):
    iso, output = Path(iso).resolve(), Path(output).resolve()
    if not iso.is_file() or iso.suffix.lower() != ".iso":
        raise ValueError("Select a game ISO file.")
    manifest = verify_payload(payload)
    destination = output / "switch" / APP
    if destination.exists():
        raise ValueError(f"An installation already exists at {destination}. Choose another output folder.")
    report("Checking your game disc. This may take several minutes.")
    iso_hash = digest(iso, report)
    if iso_hash not in manifest["supported_iso_sha256"]:
        raise ValueError("Unsupported disc. This release needs the verified European PAL Dark Chronicle ISO; US Dark Cloud 2 is not supported.")
    output.mkdir(parents=True, exist_ok=True)
    if shutil.disk_usage(output).free < manifest["minimum_free_bytes"]:
        raise ValueError("At least 12 GiB of free space is required in the output folder.")
    # Stage on the destination volume. Never modify an existing installation.
    stage = Path(tempfile.mkdtemp(prefix="ChronicleTwo-unfinished-", dir=output))
    try:
        app = stage / APP
        shutil.copytree(payload / "runtime", app)
        for name, expected in manifest["files"].items():
            if name.startswith("runtime/"):
                copied = app / Path(name).relative_to("runtime")
                if copied.stat().st_size != expected["bytes"] or digest(copied) != expected["sha256"]:
                    raise RuntimeError(f"Copy verification failed: {name}")
        game = app / "game"
        game.mkdir(exist_ok=True)
        with (stage / "packager.log").open("w", encoding="utf-8") as log:
            tool = payload / "tools/dc2data.exe"
            report("Extracting and converting game data. Please keep this window open.")
            runner([tool, "extract", iso, game / "data", "--jobs", "2"], log, report)
            report("Verifying all converted game data.")
            runner([tool, "check", game / "data"], log, report)
        for name in ("data.zip", "data.json"):
            if not (game / name).is_file():
                raise RuntimeError(f"Extractor did not produce {name}")
        report("Creating installation checksums.")
        archive = game / "data.zip"
        (game / "archive.sha256").write_text(f"{archive.stat().st_size} {digest(archive)}\n", encoding="ascii")
        files = {p.relative_to(app).as_posix(): {"bytes": p.stat().st_size, "sha256": digest(p)}
                 for p in app.rglob("*") if p.is_file()}
        receipt = {"schema": 1, "switch_version": manifest["switch_version"],
                   "iso_sha256": iso_hash, "archive_verified": True,
                   "hardware_tested": manifest["hardware_tested"], "files": files}
        (app / "installation.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")
        shutil.copy2(stage / "packager.log", app / "packager.log")
        destination.parent.mkdir(parents=True, exist_ok=True)
        # Windows rename refuses an existing destination, including a concurrent install.
        app.rename(destination)
        (stage / "packager.log").unlink(missing_ok=True)
        if stage.exists():
            stage.rmdir()
        report(f"Finished: {destination}")
        return destination
    except Exception as error:
        raise RuntimeError(f"{error}\nUnfinished files and log: {stage}") from error


def gui():
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
    root = tk.Tk()
    root.title("ChronicleTwo Switch Packager")
    root.geometry("720x440")
    root.minsize(600, 400)
    frame = ttk.Frame(root, padding=20)
    frame.pack(fill="both", expand=True)
    ttk.Label(frame, text="Prepare ChronicleTwo for Switch", font=("Segoe UI", 17)).pack(anchor="w")
    ttk.Label(frame, text="Select your European PAL Dark Chronicle ISO and an output folder.\nAllow at least 12 GiB free. Extraction can take 15 minutes or longer.").pack(anchor="w", pady=12)
    iso, output = tk.StringVar(), tk.StringVar()
    for label, variable, picker in (
        ("Game ISO", iso, lambda: filedialog.askopenfilename(filetypes=[("Disc image", "*.iso") ])),
        ("Output folder", output, filedialog.askdirectory)):
        row = ttk.Frame(frame)
        row.pack(fill="x", pady=5)
        ttk.Label(row, text=label, width=14).pack(side="left")
        ttk.Entry(row, textvariable=variable).pack(side="left", fill="x", expand=True)
        ttk.Button(row, text="Browse…", command=lambda v=variable, p=picker: v.set(p() or v.get())).pack(side="left", padx=8)
    status = tk.StringVar(value="Native Switch preview. Performance and compatibility work is ongoing.")
    ttk.Label(frame, textvariable=status, wraplength=650).pack(anchor="w", pady=12)
    bar = ttk.Progressbar(frame, mode="indeterminate")
    bar.pack(fill="x")
    events = queue.Queue()
    busy = False

    def start():
        nonlocal busy
        if not iso.get() or not output.get():
            messagebox.showerror("Choose files", "Select an ISO and output folder first.")
            return
        busy = True
        button.config(state="disabled")
        bar.start()
        selected_iso, selected_output = iso.get(), output.get()
        def work():
            try:
                result = install(selected_iso, selected_output, payload_root(), lambda s: events.put(("status", s)))
                events.put(("done", str(result)))
            except Exception as error:
                events.put(("error", str(error)))
        threading.Thread(target=work, daemon=True).start()

    def poll():
        nonlocal busy
        # Collapse noisy extractor updates, keeping the GUI responsive.
        latest = None
        while True:
            try:
                kind, text = events.get_nowait()
            except queue.Empty:
                break
            if kind == "status":
                latest = text
            else:
                busy = False
                bar.stop()
                button.config(state="normal")
                latest = "Complete." if kind == "done" else "Preparation failed."
                if kind == "done":
                    messagebox.showinfo("Installation ready", f"Created {text}\n\nCopy the output's switch folder to the SD-card root. Launch ChronicleTwo in full-application homebrew mode.")
                else:
                    messagebox.showerror("Preparation failed", text)
        if latest:
            status.set(latest[-600:])
        root.after(100, poll)

    button = ttk.Button(frame, text="Create Switch folder", command=start)
    button.pack(anchor="e", pady=15)
    def close():
        if busy:
            messagebox.showinfo("Preparation running", "Please keep the window open until preparation finishes.")
        else:
            root.destroy()
    root.protocol("WM_DELETE_WINDOW", close)
    poll()
    root.mainloop()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--iso", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.iso and args.output:
        try:
            install(args.iso, args.output, payload_root())
        except Exception as error:
            print(error, file=sys.stderr)
            sys.exit(1)
    elif args.iso or args.output:
        parser.error("--iso and --output must be supplied together")
    else:
        gui()
