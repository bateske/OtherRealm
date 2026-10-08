# SPDX-License-Identifier: GPL-2.0-or-later
"""Include the text-patcher suite in the standard tools/ test discovery."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    "otherrealm_text_patch_tests", Path(__file__).resolve().parents[1] / "tests/test_patch_text.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
TextPatchTests = module.TextPatchTests

if __name__ == "__main__":
    unittest.main()
