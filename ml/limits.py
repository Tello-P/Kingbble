"""Load limits for the heavy steps run from Python, as scripts/issues/lib.sh does.

Every command runs at the lowest CPU and I/O priority, inside a systemd user
scope with a CPU quota of `threads` cores, `mem` of memory and no swap (else
ulimit -v), so the PC stays usable. busy() lists the other heavy runs, which a
step should wait for unless forced.
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
from dataclasses import dataclass

# Same pattern as lib.sh's busy_check: issue scripts, engine binaries, Macondo.
BUSY = re.compile(
    r"^([^ ]*/)?(ba)?sh [^ ]*issue_[0-9]+_[^ ]*\.sh"
    r"|^[^ ]*(kingbble_[a-z_]+|engine_tests|macondo)( |$)"
)


@dataclass
class Limits:
    threads: int = int(
        os.environ.get("KINGBBLE_THREADS") or os.environ.get("KINGBBLE_MAX_THREADS") or 3
    )
    mem: str = os.environ.get("KINGBBLE_MEM_MAX") or os.environ.get("KINGBBLE_MAX_MEM") or "6G"

    def env(self) -> dict:
        """The environment with BLAS/OpenMP capped to `threads`."""
        env = dict(os.environ)
        for name in (
            "OMP_NUM_THREADS",
            "OPENBLAS_NUM_THREADS",
            "MKL_NUM_THREADS",
            "NUMEXPR_NUM_THREADS",
        ):
            env[name] = str(self.threads)
        return env

    def prefix(self) -> list[str]:
        """The command prefix that applies the limits."""
        nice = ["nice", "-n", "19", "ionice", "-c3"]
        if shutil.which("systemd-run") and _scopes_work():
            mem = (
                []
                if self.mem == "none"
                else ["-p", f"MemoryMax={self.mem}", "-p", "MemorySwapMax=0"]
            )
            return [
                "systemd-run",
                "--user",
                "--scope",
                "--quiet",
                *mem,
                "-p",
                f"CPUQuota={100 * self.threads}%",
                *nice,
            ]
        if self.mem == "none":
            return nice
        kb = _bytes(self.mem) // 1024
        return ["bash", "-c", f'ulimit -v {kb}; exec {" ".join(nice)} "$@"', "limits"]

    def run(self, cmd: list[str], **kw) -> subprocess.CompletedProcess:
        """Runs cmd under the limits (check=True unless given)."""
        kw.setdefault("check", True)
        return subprocess.run(self.prefix() + [str(c) for c in cmd], env=self.env(), **kw)


_SCOPES: bool | None = None


def _scopes_work() -> bool:
    global _SCOPES
    if _SCOPES is None:
        _SCOPES = (
            subprocess.run(
                ["systemd-run", "--user", "--scope", "--quiet", "true"], capture_output=True
            ).returncode
            == 0
        )
    return _SCOPES


def _bytes(size: str) -> int:
    units = {"K": 1 << 10, "M": 1 << 20, "G": 1 << 30, "T": 1 << 40}
    size = size.strip().upper()
    return int(float(size[:-1]) * units[size[-1]]) if size[-1] in units else int(size)


def busy(own: set[int] | None = None) -> list[str]:
    """Command lines of the other heavy runs (not the processes in `own`)."""
    own = (own or set()) | {os.getpid(), os.getppid()}
    out = subprocess.run(["ps", "-eo", "pid=,args="], capture_output=True, text=True).stdout
    found = []
    for line in out.splitlines():
        pid, _, cmd = line.strip().partition(" ")
        if int(pid) not in own and BUSY.search(cmd.strip()):
            found.append(line.strip())
    return found
