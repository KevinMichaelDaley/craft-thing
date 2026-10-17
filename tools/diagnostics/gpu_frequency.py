"""Read-only Intel GPU frequency sampling; never changes driver settings."""

import argparse
import glob
import json
from pathlib import Path
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--root", type=Path)
    parser.add_argument("--interval", type=float, default=.002)
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
        while process.poll() is None:
            output.write(json.dumps({"seconds": time.monotonic() - start,
                                     "mhz": snapshot(root)}) + "\n")
            output.flush()
            time.sleep(args.interval)
        output.write(json.dumps({"seconds": time.monotonic() - start,
                                 "returncode": process.returncode}) + "\n")
    return process.returncode


if __name__ == "__main__":
    sys.exit(main())
