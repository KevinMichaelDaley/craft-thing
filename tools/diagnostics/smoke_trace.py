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
TAIL = re.compile(r"Acoustic GPU tail finish([\d.]+) commit([\d.]+) ms")
FRAME = re.compile(r"app-frame,(\d+),([\d.]+)ms")
ACOUSTIC = re.compile(r"Acoustic dt([\deE.+-]+) batch(\d+) residual([\deE.+-]+).* tiles(\d+) advected(\d+)")
INTERVAL = re.compile(r"Gas interval dt([\deE.+-]+) accepted(\d+) retries(\d+) submits(\d+) queue([\d.]+) wait([\d.]+) storage(\d+)")
WAITS = re.compile(r"Present waits acquire([\d.]+) fence([\d.]+)")
MEMORY = re.compile(r"app-memory,\d+,rss_kb(\d+),minflt(\d+),majflt(\d+)")
INPUT = re.compile(r"app-input,\d+,events(\d+),wall([\d.]+),cpu([\d.]+)")


def counters():
    return dict(accepted=0, retries=0, retry_lines=0, gas_submits=0,
                simulated_seconds=0.0, queue_ms=0.0, gas_wait_ms=0.0,
                storage_bytes=0, acquire_ms=0.0, present_fence_ms=0.0,
                solve_tiles_peak=0, advection_tiles_peak=0,
                residual_peak=0.0, pressure_batches_peak=0,
                rss_kb=0, minor_faults=0, major_faults=0,
                input_events=0, input_cpu_ms=0.0, input_wait_ms=0.0)


def frames(lines):
    result = []
    init = pressure = residual = finish = commit = 0.0
    substeps = submissions = 0
    extra = counters()
    intervals = 0
    for line in lines:
        match = INPUT.search(line)
        if match:
            extra["input_events"] += int(match[1])
            extra["input_cpu_ms"] += float(match[3])
            extra["input_wait_ms"] += max(0.0, float(match[2]) - float(match[3]))
        match = TAIL.search(line)
        if match:
            finish += float(match[1])
            commit += float(match[2])
        if line.startswith("Gas retry "):
            extra["retry_lines"] += 1
        match = ACOUSTIC.search(line)
        if match:
            extra["pressure_batches_peak"] = max(extra["pressure_batches_peak"], int(match[2]))
            extra["residual_peak"] = max(extra["residual_peak"], float(match[3]))
            extra["solve_tiles_peak"] = max(extra["solve_tiles_peak"], int(match[4]))
            extra["advection_tiles_peak"] = max(extra["advection_tiles_peak"], int(match[5]))
        match = INTERVAL.search(line)
        if match:
            intervals += 1
            extra["simulated_seconds"] += float(match[1])
            for name, group in (("accepted", 2), ("retries", 3), ("gas_submits", 4)):
                extra[name] += int(match[group])
            extra["queue_ms"] += float(match[5])
            extra["gas_wait_ms"] += float(match[6])
            extra["storage_bytes"] = max(extra["storage_bytes"], int(match[7]))
        match = WAITS.search(line)
        if match:
            extra["acquire_ms"] += float(match[1])
            extra["present_fence_ms"] += float(match[2])
        match = MEMORY.search(line)
        if match:
            extra["rss_kb"], extra["minor_faults"], extra["major_faults"] = map(int, match.groups())
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
            gpu = init + pressure + residual + finish + commit
            if not intervals:
                extra["retries"] = extra["retry_lines"]
                extra["accepted"] = max(0, substeps-extra["retry_lines"])
            result.append(dict(frame=index, total_ms=total, substeps=substeps,
                               submissions=submissions, init_ms=init,
                               pressure_ms=pressure, residual_ms=residual, finish_ms=finish, commit_ms=commit,
                               gpu_ms=gpu, remaining_ms=total-gpu, **extra))
            init = pressure = residual = finish = commit = 0.0
            substeps = submissions = 0
            extra = counters()
            intervals = 0
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
