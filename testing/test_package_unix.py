"""Linux packaging smoke test; requires patchelf and uses /bin/ls as an ELF fixture."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


@unittest.skipUnless(sys.platform == "linux", "Requires a Linux ELF loader")
class PackageTest(unittest.TestCase):
    def test_relocated_package(self):
        repo = Path(__file__).resolve().parent.parent
        with tempfile.TemporaryDirectory(prefix="lhat-package-test-") as temporary:
            root = Path(temporary)
            for name in ("build", "deps", "SDL"):
                (root / name).mkdir()
            (root / "SDL/LICENSE.txt").write_text("test fixture", encoding="utf-8")
            shutil.copy2("/bin/ls", root / "build/love")
            subprocess.run([
                sys.executable, str(repo / "scripts/package-unix.py"),
                str(root / "build"), str(root / "package"), "--deps", str(root / "deps"),
            ], check=True)
            shutil.rmtree(root / "build")
            relocated = root / "relocated package"
            shutil.move(str(root / "package"), str(relocated))
            executable = relocated / "bin/love"
            subprocess.run([str(executable), "--version"], check=True, capture_output=True)
            linked = subprocess.check_output(["ldd", str(executable)], text=True)
            self.assertNotIn("not found", linked)
            self.assertIn("relocated package/bin/../lib/", linked)
            self.assertTrue((relocated / "licenses/SDL.txt").is_file())


if __name__ == "__main__":
    unittest.main()
