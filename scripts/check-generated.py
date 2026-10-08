"""Verify checked-in VM headers with a native full engine on Unix."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

repo = Path(__file__).resolve().parent.parent
engine = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix="lhat-generated-") as temporary:
    work = Path(temporary)
    embedded = work / "embedded"
    embedded.mkdir()
    for args in (
        ("--dump-signatures", str(work / "sigs.bin"), "testing/lh/hello"),
        ("--dump-embedded", str(embedded) + "/", "testing/lh/hello"),
        ("--dump-embedded", str(embedded) + "/"),
    ):
        subprocess.run([engine, *args], cwd=repo, check=True, timeout=120)
    for header, binary in (
        ("Signatures.h", work / "sigs.bin"),
        ("BootBinary.h", embedded / "Boot.lh.bin"),
        ("NogameBinary.h", embedded / "main.lh.bin"),
    ):
        text = (repo / "src/lh" / header).read_text(encoding="utf-8")
        stored = bytes(int(byte, 16) for byte in re.findall(r"0x([0-9a-fA-F]{2}),", text))
        if stored != binary.read_bytes():
            raise SystemExit(f"{header} is stale: regenerate with the pinned lhat revision")
        print(f"{header}: matches")
