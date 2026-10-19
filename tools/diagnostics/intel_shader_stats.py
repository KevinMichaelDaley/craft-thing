#!/usr/bin/env python3
"""Collect Intel compiler summaries without retaining megabytes of assembly."""
import argparse
import os
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        parser.error("a bounded diagnostic command is required")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ)
    environment["INTEL_DEBUG"] = ",".join(filter(None, [environment.get("INTEL_DEBUG"), "cs"]))
    summary = re.compile(r"shader|spills|fills|GRF|unsupported|unknown", re.IGNORECASE)
    with args.output.open("w") as log:
        process = subprocess.Popen(command, env=environment, stderr=subprocess.PIPE,
                                   text=True, errors="replace")
        for line in process.stderr:
            if summary.search(line):
                log.write(line)
                log.flush()
        result = process.wait()
        log.write(f"\ncommand exit {result}\n")
    return result


if __name__ == "__main__":
    raise SystemExit(main())
