from pathlib import Path
import struct
import unittest


def corner_capacity(path, pixel_only):
    raw = Path(path).read_bytes()
    words = struct.unpack(f"<{len(raw) // 4}I", raw)
    names, nodes, spec_ids = {}, {}, {}
    offset = 5
    while offset < len(words):
        size, opcode = words[offset] >> 16, words[offset] & 65535
        if not size or offset + size > len(words):
            raise ValueError("invalid SPIR-V instruction")
        args = words[offset + 1:offset + size]
        if opcode == 5:
            name = struct.pack(f"<{len(args)-1}I", *args[1:]).split(b"\0", 1)[0].decode()
            names[name] = args[0]
        if opcode in (28, 30):
            nodes[args[0]] = opcode, args
        if opcode in (41, 42, 43, 48, 49, 50, 52):
            nodes[args[1]] = opcode, args
        if opcode == 71 and len(args) == 3 and args[1] == 1:
            spec_ids[args[0]] = args[2]
        offset += size

    def value(identifier):
        if spec_ids.get(identifier) == 1:
            return int(pixel_only)
        opcode, args = nodes[identifier]
        if opcode in (41, 48):
            return 1
        if opcode in (42, 49):
            return 0
        if opcode in (43, 50):
            return args[2]
        if opcode == 52 and args[2] == 169:
            return value(args[4] if value(args[3]) else args[5])
        raise ValueError(f"unsupported constant expression {opcode}")

    _, polygon = nodes[names["Polygon"]]
    _, vertices = nodes[polygon[1]]
    return value(vertices[2])


class ContactPolygonStorageTests(unittest.TestCase):
    def test_contact_workgroups_spread_small_manifolds(self):
        for variant in ("", "runtime/"):
            for module in ("contacts", "contacts_pixel"):
                raw = Path(f"build/shaders/{variant}{module}.comp.spv").read_bytes()
                words = struct.unpack(f"<{len(raw) // 4}I", raw)
                offset = 5
                sizes = []
                while offset < len(words):
                    size, opcode = words[offset] >> 16, words[offset] & 65535
                    args = words[offset + 1:offset + size]
                    if opcode == 16 and args[1] == 17:
                        sizes.append(tuple(args[2:]))
                    offset += size
                self.assertEqual(sizes, [(16, 1, 1)], module)

    def test_contact_storage_matches_the_specialized_geometry(self):
        for variant in ("", "runtime/"):
            with self.subTest(variant=variant):
                generic = f"build/shaders/{variant}contacts.comp.spv"
                pixels = f"build/shaders/{variant}contacts_pixel.comp.spv"
                self.assertEqual(corner_capacity(generic, False), 8)
                self.assertEqual(corner_capacity(pixels, True), 4)


if __name__ == "__main__":
    unittest.main()
