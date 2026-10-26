#!/usr/bin/env python3
"""Build test artifacts in parallel without running GPU tests concurrently."""

import argparse
from pathlib import Path
import re
import subprocess
import sys


def build_targets(database):
    """Resolve file prerequisites through phony test targets, without recipes."""
    graph = {}
    for line in database.splitlines():
        match = re.fullmatch(r"([^\s:=#]+):\s*(.*)", line)
        if match:
            graph.setdefault(match[1], []).extend(p for p in match[2].split() if p != "|")
    if "test" not in graph:
        raise ValueError("Make database has no test target")
    pending, visited, targets = ["test"], set(), set()
    while pending:
        name = pending.pop()
        if name in visited:
            continue
        visited.add(name)
        if name.startswith("build/") and "%" not in name and not name.endswith("/"):
            targets.add(name)
        pending.extend(graph.get(name, []))
    return sorted(targets)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()
    if args.jobs <= 0:
        parser.error("--jobs must be positive")
    root = Path(__file__).resolve().parents[1]
    wrapper = root / "tools/with-vulkan"
    probe = subprocess.run([str(wrapper), "make", "-qp", "test"], cwd=root,
                           capture_output=True, text=True, check=False)
    if probe.returncode not in (0, 1):
        sys.stderr.write(probe.stderr)
        return probe.returncode
    targets = build_targets(probe.stdout)
    if not targets:
        raise ValueError("Test target has no build artifacts")
    if args.list:
        print("\n".join(targets))
        return 0
    print(f"Prebuilding {len(targets)} artifacts; test recipes are not executed", flush=True)
    return subprocess.call([str(wrapper), "make", f"-j{args.jobs}", *targets], cwd=root)


if __name__ == "__main__":
    raise SystemExit(main())
