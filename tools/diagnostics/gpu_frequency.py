"""Read-only Intel GPU frequency sampling; never changes driver settings."""

import argparse
import glob
import json
from pathlib import Path
import re
import subprocess
import sys
import time


def snapshot(root):
    result = {}
    for path in sorted(root.glob("gt_*_freq_mhz")):
        try:
            result[path.name] = int(path.read_text().strip())
        except (OSError, ValueError):
            continue
    return result


def drm_clients(root=Path("/proc")):
    clients = {}
    for path in root.glob("[0-9]*/fdinfo/*"):
        try:
            contents = path.read_text()
            client = re.search(r"^drm-client-id:\s*(\d+)$", contents, re.M)
            engines = {name: int(value) for name, value in re.findall(
                r"^drm-engine-([^:]+):\s*(\d+) ns$", contents, re.M)}
            if not client or not engines:
                continue
            device = re.search(r"^drm-pdev:\s*(\S+)$", contents, re.M)
            if not device:
                continue
            process = path.parent.parent
            name = (process / "comm").read_text().strip()
            pid = int(process.name)
        except (OSError, ValueError):
            continue
        key = device[1] + "/" + client[1]
        entry = clients.setdefault(key, {"pids": set(), "names": set(), "engines_ns": {}})
        entry["pids"].add(pid)
        entry["names"].add(name)
        for engine, value in engines.items():
            entry["engines_ns"][engine] = max(value, entry["engines_ns"].get(engine, 0))
    for entry in clients.values():
        entry["pids"] = sorted(entry["pids"])
        entry["names"] = sorted(entry["names"])
    return clients


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--root", type=Path)
    parser.add_argument("--interval", type=float, default=.002)
    parser.add_argument("--drm-clients", action="store_true",
                        help="Also sample per-client engine time once per second")
    parser.add_argument("command", nargs=argparse.REMAINDER,
                        help="Command with an explicit external timeout")
    args = parser.parse_args()
    command = args.command
    if command and command[0] == "--":
        command = command[1:]
    if not command or args.interval <= 0:
        parser.error("a bounded command and positive interval are required")
    roots = sorted(glob.glob("/sys/class/drm/card*/gt_act_freq_mhz"))
    root = args.root or (Path(roots[0]).parent if roots else None)
    if root is None:
        parser.error("no readable Intel frequency directory found; supply --root")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    start = time.monotonic()
    with args.output.open("w") as output:
        output.write(json.dumps({"command": command, "root": str(root),
                                 "interval_seconds": args.interval}) + "\n")
        process = subprocess.Popen(command)
        client_sample = start
        while process.poll() is None:
            now = time.monotonic()
            sample = {"seconds": now - start, "mhz": snapshot(root)}
            if args.drm_clients and now >= client_sample:
                sample["drm_clients"] = drm_clients()
                client_sample = now + 1.
            output.write(json.dumps(sample) + "\n")
            output.flush()
            time.sleep(args.interval)
        output.write(json.dumps({"seconds": time.monotonic() - start,
                                 "returncode": process.returncode}) + "\n")
    return process.returncode


if __name__ == "__main__":
    sys.exit(main())
