#!/usr/bin/env python3
"""Associate Intel compiler statistics with optional engine shader markers."""
import argparse
import csv


def rows(lines):
    return []


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log")
    args = parser.parse_args()
    with open(args.log, encoding="utf-8", errors="replace") as source:
        result = rows(source)
    if result:
        writer = csv.DictWriter(__import__("sys").stdout, fieldnames=result[0].keys())
        writer.writeheader()
        writer.writerows(result)
