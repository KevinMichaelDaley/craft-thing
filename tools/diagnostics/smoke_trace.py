#!/usr/bin/env python3
"""Account existing GPU trace lines against line-buffered displayed frames.

Capture with stdbuf -oL, DC_GAS_TRACE=1 and DC_FRAME_TIMINGS=1. The first
reported frame includes preceding warmup trace data; exclude it from comparisons.
This is diagnostic accounting, never a replacement for the uninstrumented gate.
"""
import argparse
import csv
import re
import sys

GPU = re.compile(r"Acoustic GPU init([\d.]+) pressure-group([\d.]+) residual([\d.]+) ms")
FRAME = re.compile(r"app-frame,(\d+),([\d.]+)ms")


def frames(lines):
    result = []
    init = pressure = residual = 0.0
    substeps = submissions = 0
    for line in lines:
        match = GPU.search(line)
        if match:
            first, second, third = map(float, match.groups())
            init += first
            pressure += second
            residual += third
            submissions += 1
            substeps += first > 0
        match = FRAME.search(line)
        if match:
            index, total = int(match[1]), float(match[2])
            gpu = init + pressure + residual
            result.append(dict(frame=index, total_ms=total, substeps=substeps,
                               submissions=submissions, init_ms=init,
                               pressure_ms=pressure, residual_ms=residual,
                               gpu_ms=gpu, remaining_ms=total-gpu))
            init = pressure = residual = 0.0
            substeps = submissions = 0
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log")
    args = parser.parse_args()
    with open(args.log, encoding="utf-8", errors="replace") as source:
        rows = frames(source)
    if not rows:
        parser.error("No complete displayed frame records found")
    writer = csv.DictWriter(sys.stdout, fieldnames=rows[0].keys())
    writer.writeheader()
    writer.writerows(rows)


if __name__ == "__main__":
    main()
