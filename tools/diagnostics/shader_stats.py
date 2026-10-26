#!/usr/bin/env python3
"""Associate Intel compiler statistics with optional engine shader markers."""
import argparse
import csv
import re
import sys

STATS = re.compile(r"SIMD(\d+) shader: (\d+) instructions\. (\d+) loops\. (\d+) cycles\. (\d+):(\d+) spills:fills, (\d+) sends")


def rows(lines):
    shader, result = "unknown", []
    for line in lines:
        if line.startswith("Shader compile: "):
            shader = line.removeprefix("Shader compile: ").strip()
        match = STATS.search(line)
        if match:
            result.append(dict(shader=shader, **dict(zip(
                ("simd", "instructions", "loops", "cycles", "spills", "fills", "sends"),
                map(int, match.groups())))))
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log")
    args = parser.parse_args()
    with open(args.log, encoding="utf-8", errors="replace") as source:
        result = rows(source)
    if result:
        writer = csv.DictWriter(sys.stdout, fieldnames=result[0].keys())
        writer.writeheader()
        writer.writerows(result)
