"""Read the bounded Switch effect history, including while the game runs."""
import argparse
import datetime
import ftplib
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="192.168.0.100")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    out = root / "port/build/switch/hardware-pulls" / ("effect-history-" + stamp)
    out.mkdir(parents=True, exist_ok=True)
    with ftplib.FTP() as ftp:
        ftp.connect(args.host, 21, timeout=12)
        ftp.login()
        ftp.cwd("/switch/ChronicleTwo-PC-Switch/game")
        names = sorted({Path(name).name for name in ftp.nlst()})
        snapshots = []
        for name in names:
            if name != "effect-state.txt" and not re.fullmatch(r"effect-history-\d{2}\.txt", name):
                continue
            try:
                data = bytearray()
                ftp.retrbinary("RETR " + name, data.extend)
                (out / name).write_bytes(data)
                match = re.search(rb"session (\d+) sequence (\d+)", data)
                if match and name != "effect-state.txt":
                    snapshots.append((int(match[1]), int(match[2]), name, data.decode(errors="replace")))
            except ftplib.error_perm as error:
                print(name, error)  # A currently open slot can be skipped safely.
        ordered = "\n\n".join(f"FILE {name}\n{text}" for _, _, name, text in sorted(snapshots))
        (out / "ordered-history.txt").write_text(ordered, encoding="utf-8")
    print(f"Retrieved {len(snapshots)} retained snapshots to {out}")


if __name__ == "__main__":
    main()
