#!/usr/bin/env python3
"""The recompute variant must actually release its per-cell shared requests."""
import subprocess
import unittest

class RequestStorageTest(unittest.TestCase):
    def test_recompute_artifact_has_no_shared_request_array(self):
        text = subprocess.check_output([
            "spirv-dis", "build/shaders/fluid_transport_recompute_16.comp.spv"], text=True)
        self.assertNotIn('OpName %cached_requests "cached_requests"', text)

if __name__ == "__main__":
    unittest.main()
