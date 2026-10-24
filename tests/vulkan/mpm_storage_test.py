from pathlib import Path
import struct
import unittest


def block_members(path):
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
    return {name: {d for target, member, d in decorations if target == identifier and member == 0}
            for name, identifier in names.items()}


class MpmStorageTests(unittest.TestCase):
    def test_disjoint_particle_pools_and_scratch_allow_nonaliasing_access(self):
        for path in ("build/shaders/mpm.comp.spv", "build/shaders/runtime/mpm.comp.spv"):
            blocks = block_members(path)
            for name in ("ChunkCells", "Pages", "Occupancy", "FaceVelocity", "Particles",
                         "ParticleCounts", "Proposals", "OutputParticles", "Grid",
                         "GridForce", "GridVelocity", "Accept", "Activity",
                         "ComponentLabels", "ComponentSizes"):
                with self.subTest(path=path, block=name):
                    self.assertIn(19, blocks[name], "disjoint storage must be Restrict")


if __name__ == "__main__":
    unittest.main()
