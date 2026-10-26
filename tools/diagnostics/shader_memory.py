#!/usr/bin/env python3
"""Report statically declared SPIR-V workgroup storage (not runtime profiling)."""
import argparse
import struct
from pathlib import Path


def _instructions(data):
    if len(data) < 20 or len(data) % 4:
        raise ValueError("Invalid SPIR-V byte length")
    words = struct.unpack("<" + "I" * (len(data) // 4), data)
    if words[0] != 0x07230203:
        raise ValueError("Invalid SPIR-V magic")
    at = 5
    while at < len(words):
        length, opcode = words[at] >> 16, words[at] & 65535
        if length == 0 or at + length > len(words):
            raise ValueError("Invalid SPIR-V instruction")
        args = words[at + 1:at + length]
        yield opcode, args
        at += length


def workgroup_bytes(data):
    types, constants, variables = {}, {}, []
    for opcode, args in _instructions(data):
        if opcode in (21, 22, 23, 28, 30, 32):
            types[args[0]] = (opcode, args[1:])
        elif opcode == 43:
            constants[args[1]] = args[2]
        elif opcode == 59 and args[2] == 4:
            variables.append(args[0])

    def size(type_id):
        opcode, args = types[type_id]
        if opcode in (21, 22):
            return args[0] // 8
        if opcode == 23:
            return size(args[0]) * args[1]
        if opcode == 28:
            return size(args[0]) * constants[args[1]]
        if opcode == 30:
            return sum(size(member) for member in args)
        if opcode == 32:
            return size(args[1])
        raise ValueError("Unsupported workgroup type")

    return sum(size(variable) for variable in variables)


def local_size(data):
    constants, dimensions = {}, None
    for opcode, args in _instructions(data):
        if opcode == 43:
            constants[args[1]] = args[2]
        elif opcode == 16 and args[1] == 17:
            return tuple(args[2:5])
        elif opcode == 331 and args[1] == 38:
            dimensions = args[2:5]
    if dimensions is None:
        raise ValueError("No fixed compute local size")
    return tuple(constants[dimension] for dimension in dimensions)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("shader", type=Path)
    parser.add_argument("--local-size", action="store_true")
    args = parser.parse_args()
    data = args.shader.read_bytes()
    print(*local_size(data)) if args.local_size else print(workgroup_bytes(data))
