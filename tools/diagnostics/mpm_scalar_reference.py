"""Generate a pinned historical benchmark; never used by runtime pipelines."""
import hashlib
import json
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]
SHADER_REVISION = "b007b143169c8e49514eaf9bc7915a4b268fcbe8"
TEST_REVISION = "1da1d95d6154660a169568305b34b4335281a89c"
OUTPUT = ROOT / "build/diagnostics"


def historical(revision, path):
    return subprocess.check_output(["git", "show", revision + ":" + path],
                                   cwd=ROOT, text=True)


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    shader = historical(SHADER_REVISION, "shaders/sim/mpm_active.comp")
    shader = shader.replace('"rigid_body.glsl"', '"mpm_scalar_body.glsl"')
    body = historical(SHADER_REVISION, "shaders/sim/rigid_body.glsl")
    body = body.replace('"../../include/dungeoncraft/material_density.h"',
                        '"mpm_scalar_density.h"')
    test = historical(TEST_REVISION, "tests/vulkan/mpm_compaction_tests.c")
    original = '"build/shaders/mpm_active.comp.spv"'
    if test.count(original) != 1:
        raise SystemExit("historical fixture shader path changed")
    test = test.replace(original, '"build/diagnostics/mpm_scalar_reference.spv"')
    sources = {"mpm_scalar_reference.comp": shader, "mpm_scalar_body.glsl": body,
               "mpm_scalar_density.h": historical(SHADER_REVISION, "include/dungeoncraft/material_density.h"),
               "mpm_scalar_reference_tests.c": test}
    for name, source in sources.items():
        (OUTPUT / name).write_text(source)
    metadata = {"shader_revision": SHADER_REVISION, "test_revision": TEST_REVISION,
                "probe_sha256": hashlib.sha256((ROOT / "tools/diagnostics/vulkan_probe.c").read_bytes()).hexdigest(),
                "sources": {name: hashlib.sha256(source.encode()).hexdigest()
                            for name, source in sources.items()}}
    (OUTPUT / "mpm_scalar_reference.json").write_text(json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
    main()
