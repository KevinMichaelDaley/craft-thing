#!/usr/bin/env python3
"""Explicit line storage uses physical coordinates in every gas shader."""
import struct
import unittest

from shader_memory_tests import ROOT, module


def shader_functions(path):
    names = {}
    calls = {}
    for opcode, args in module._instructions(path.read_bytes()):
        if opcode == 5:  # OpName
            name = struct.pack(f"<{len(args) - 1}I", *args[1:]).split(b"\0", 1)[0]
            names[args[0]] = name.decode().split("(", 1)[0]
        elif opcode == 54:  # OpFunction
            function = args[1]
            calls[function] = set()
        elif opcode == 57:  # OpFunctionCall
            calls[function].add(args[2])
    return names, calls


class ExplicitOrderTests(unittest.TestCase):
    def assert_explicit_ignores_implicit_direction(self, shader, entry):
        path = ROOT / f"build/shaders/gas_{shader}.comp.spv"
        names, calls = shader_functions(path)
        pending = [identifier for identifier in calls if names.get(identifier) == entry]
        self.assertEqual(len(pending), 1, f"Expected one {entry} function in {shader}")
        visited = set()
        while pending:
            function = pending.pop()
            self.assertNotEqual(names.get(function), "transport_direction",
                                "explicit line traversal depends on the implicit scan direction")
            visited.add(function)
            pending.extend(calls[function] - visited)

    def test_standalone_explicit(self):
        for suffix in ("", "_wide", "_large"):
            with self.subTest(variant=suffix):
                self.assert_explicit_ignores_implicit_direction("transport_explicit" + suffix, "main")

    def test_fused_hybrid_explicit_stage(self):
        for axis in ("", "_x", "_y"):
            sizes = ("", "_wide", "_large", "_wide_local") if axis else ("", "_wide", "_large")
            for size in sizes:
                with self.subTest(axis=axis, size=size):
                    self.assert_explicit_ignores_implicit_direction(
                        "transport_hybrid" + axis + size, "gas_explicit_run")


if __name__ == "__main__":
    unittest.main()
