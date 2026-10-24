from pathlib import Path
import struct
import unittest


def root_decorations(path, block="RuntimeBodyRoot"):
    raw = Path(path).read_bytes()
    words = struct.unpack(f"<{len(raw) // 4}I", raw)
    names, decorations = {}, set()
    offset = 5
    while offset < len(words):
        length, opcode = words[offset] >> 16, words[offset] & 65535
        if not length or offset + length > len(words):
            raise ValueError("invalid SPIR-V instruction")
        operands = words[offset + 1:offset + length]
        if opcode == 5:
            name = struct.pack(f"<{len(operands)-1}I", *operands[1:]).split(b"\0", 1)[0]
            names[name.decode()] = operands[0]
        if opcode == 72 and len(operands) >= 3:
            decorations.add(tuple(operands[:3]))
        offset += length
    root = names[block]
    return {decoration for target, member, decoration in decorations
            if target == root and member == 0}


class RuntimeRootStorageTests(unittest.TestCase):
    def test_consumers_promise_readonly_nonaliasing_root_storage(self):
        for shader in ("rigid_solver", "rigid", "pattern", "mpm", "scene"):
            with self.subTest(shader=shader):
                decorations = root_decorations(f"build/shaders/runtime/{shader}.comp.spv")
                self.assertIn(24, decorations, "root must be NonWritable")
                self.assertIn(19, decorations, "root must be Restrict")

    def test_explicit_index_writer_can_publish_the_live_count(self):
        decorations = root_decorations("build/shaders/runtime_body_index.comp.spv")
        self.assertNotIn(24, decorations)

    def test_consumers_cache_directory_addresses_and_immutable_geometry(self):
        for shader in ("rigid_solver", "rigid", "pattern", "scene"):
            for block in ("BodyStorageDirectory", "BodyStorageGeometry"):
                with self.subTest(shader=shader, block=block):
                    decorations = root_decorations(f"build/shaders/runtime/{shader}.comp.spv", block)
                    self.assertIn(24, decorations)
                    self.assertIn(19, decorations)

    def test_index_writer_retains_directory_free_mask_updates(self):
        decorations = root_decorations("build/shaders/runtime_body_index.comp.spv", "BodyStorageDirectory")
        self.assertNotIn(24, decorations)

    def test_selected_consumers_keep_record_slabs_disjoint_and_writable(self):
        for shader in ("rigid_solver", "rigid", "pattern", "scene"):
            with self.subTest(shader=shader):
                decorations = root_decorations(f"build/shaders/runtime/{shader}.comp.spv", "BodyStorageRecords")
                self.assertIn(19, decorations)
                self.assertNotIn(24, decorations)


if __name__ == "__main__":
    unittest.main()
