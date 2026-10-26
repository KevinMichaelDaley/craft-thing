#!/usr/bin/env python3
"""Report statically declared SPIR-V workgroup storage (not runtime profiling)."""
import argparse
import struct
from pathlib import Path


def workgroup_bytes(data):
    if len(data) < 20 or len(data) % 4:
        raise ValueError("Invalid SPIR-V byte length")
    words = struct.unpack("<" + "I" * (len(data) // 4), data)
    if words[0] != 0x07230203:
        raise ValueError("Invalid SPIR-V magic")
    types, constants, variables = {}, {}, []
    at = 5
    while at < len(words):
        length, opcode = words[at] >> 16, words[at] & 65535
        if length == 0 or at + length > len(words):
            raise ValueError("Invalid SPIR-V instruction")
        args = words[at + 1:at + length]
        if opcode in (21, 22, 23, 28, 30, 32):
            types[args[0]] = (opcode, args[1:])
        elif opcode == 43:
            constants[args[1]] = args[2]
        elif opcode == 59 and args[2] == 4:
            variables.append(args[0])
        at += length

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


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("shader", type=Path)
    args = parser.parse_args()
    print(workgroup_bytes(args.shader.read_bytes()))
