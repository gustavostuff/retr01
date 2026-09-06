"""Run galette and interpret fit success / product-term overflow."""

from __future__ import annotations

import os
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path

from . import ROOT


@dataclass(frozen=True)
class FitResult:
    ok: bool
    returncode: int
    stdout: str
    stderr: str
    jed_path: Path | None

    @property
    def too_many_products(self) -> bool:
        blob = f"{self.stdout}\n{self.stderr}".lower()
        return "too many product terms" in blob


def galette_bin() -> Path:
    env = os.environ.get("GALETTE")
    if env:
        p = Path(env)
        if p.is_file() and os.access(p, os.X_OK):
            return p
    cached = ROOT / ".cache" / "galette"
    if cached.is_file() and os.access(cached, os.X_OK):
        return cached
    # Bootstrap via ensure script.
    script = ROOT / "ensure_galette.sh"
    out = subprocess.check_output(["bash", str(script)], text=True).strip().splitlines()[-1]
    p = Path(out)
    if not p.is_file():
        raise FileNotFoundError(f"galette missing after ensure: {out}")
    return p


def fit_pld(pld: Path, *, work_dir: Path | None = None) -> FitResult:
    """Assemble one .pld. Copies into a temp dir so sources stay clean."""
    pld = pld.resolve()
    galette = galette_bin()
    if work_dir is None:
        tmp = tempfile.TemporaryDirectory(prefix="pld_fit_")
        work = Path(tmp.name)
        cleanup = tmp
    else:
        work = work_dir
        cleanup = None
    try:
        dest = work / pld.name
        dest.write_text(pld.read_text(encoding="utf-8"), encoding="utf-8")
        proc = subprocess.run(
            [str(galette), str(dest)],
            cwd=str(work),
            capture_output=True,
            text=True,
        )
        jed = work / (pld.stem + ".jed")
        return FitResult(
            ok=proc.returncode == 0 and jed.is_file(),
            returncode=proc.returncode,
            stdout=proc.stdout,
            stderr=proc.stderr,
            jed_path=jed if jed.is_file() else None,
        )
    finally:
        if cleanup is not None:
            cleanup.cleanup()
