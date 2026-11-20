"""Local web interface for Kingbble: serves ui/static and forwards /api calls to
the engine bridge (build/engine/kingbble_bridge), one game per visitor.

    python ui/server.py [--lexica lexica/] [--lexicon FILE ...]
        [--human-params models/human_model.txt] [--threads 2] [--port 8765]
        [--max-sessions 8] [--idle-minutes 30] [--max-busy N]

Each visitor (a cookie) gets its own engine process, stopped after --idle-minutes
without use or when --max-sessions are open and a newer visitor arrives. At most
--max-busy commands run at once, so a public server cannot be flooded.

Every word list in lexica/ (.txt or .kwg) can be chosen in the page; its leaves are
the file with the same name and .klv2, .klv or .csv. Then open http://localhost:8765.
Only the standard library is used.
"""

from __future__ import annotations

import argparse
import ipaddress
import json
import os
import secrets
import signal
import subprocess
import sys
import threading
import time
from http.cookies import SimpleCookie
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
STATIC = Path(__file__).resolve().parent / "static"
# Commands the page may send; anything else is refused before reaching the engine.
COMMANDS = {
    "new",
    "state",
    "play",
    "check",
    "exchange",
    "pass",
    "bot",
    "hint",
    "undo",
    "view",
    "rewind",
    "gcg",
}


class Bridge:
    """The engine process, answering one JSON line per command line."""

    def __init__(self, argv: list[str]):
        self.argv = argv
        self.lock = threading.Lock()
        self.proc: subprocess.Popen[str] | None = None
        self.info: dict = {}
        self.start()

    def start(self) -> None:
        self.proc = subprocess.Popen(
            self.argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1
        )
        self.info = self._read()

    def _read(self) -> dict:
        assert self.proc and self.proc.stdout
        line = self.proc.stdout.readline()
        if not line:
            raise RuntimeError("the engine stopped (see its messages above)")
        return json.loads(line)

    def ask(self, line: str) -> dict:
        with self.lock:
            assert self.proc and self.proc.stdin
            if self.proc.poll() is not None:
                self.start()
                return {"ok": False, "error": "the engine restarted, start a new game"}
            self.proc.stdin.write(line + "\n")
            self.proc.stdin.flush()
            return self._read()

    def stop(self) -> None:
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.proc.kill()


def command_line(body: dict) -> str:
    cmd = str(body.get("cmd", ""))
    if cmd not in COMMANDS:
        raise ValueError(f"unknown command: {cmd}")
    args = [str(a) for a in body.get("args", [])]
    for a in args:
        if not a or any(c.isspace() for c in a):
            raise ValueError("bad argument")
    return " ".join([cmd, *args])


def find_lexica(
    directory: Path, explicit: list[str], use: dict[str, str] | None = None
) -> dict[str, dict]:
    """Word lists (.txt or .kwg) by name, each with its leaves: the published table beside
    it (same name, .klv2, .klv or .csv) or our own (NAME.own.klv2), as `use` picks."""
    out: dict[str, dict] = {}
    paths = [Path(p) for p in explicit]
    if directory.is_dir():
        paths += sorted(p for p in directory.iterdir() if p.suffix in (".txt", ".kwg"))
    for path in paths:
        if (
            not path.is_file()
            or path.stem in out
            or "freq" in path.stem
            or path.stem.endswith(".own")
        ):
            continue
        default = next(
            (q for ext in (".klv2", ".klv", ".csv") if (q := path.with_suffix(ext)).is_file()), None
        )
        own = path.with_name(f"{path.stem}.own.klv2")
        own = own if own.is_file() else None
        choice = (use or {}).get(path.stem)
        kind = (
            "own" if own and (choice == "own" or not default) else "default" if default else "none"
        )
        leaves = own if kind == "own" else default
        out[path.stem] = {
            "path": str(path),
            "leaves": str(leaves) if leaves else None,
            "kind": kind,
            "has_own": own is not None,
        }
    return out


class Session:
    """One visitor: its own engine process and the lexicon it plays with."""

    def __init__(self, lexicon: str):
        self.lexicon = lexicon
        self.bridge: Bridge | None = None
        self.last = time.monotonic()


class Engines:
    """One bridge per visitor and lexicon, started when first used."""

    def __init__(
        self,
        bridge: str,
        lexica: dict[str, dict],
        human: str | None,
        threads: int,
        max_sessions: int = 8,
        idle_s: float = 1800,
        max_busy: int = 0,
    ):
        self.bridge, self.lexica, self.human, self.threads = bridge, lexica, human, threads
        self.default = next(iter(lexica))
        self.sessions: dict[str, Session] = {}
        self.max_sessions, self.idle_s = max(1, max_sessions), idle_s
        busy = max_busy or max(1, (os.cpu_count() or 2) // max(1, threads))
        self.busy = threading.BoundedSemaphore(busy)
        self.lock = threading.Lock()

    def bots(self, name: str) -> list[str]:
        bots = ["greedy"]
        if self.lexica[name]["leaves"]:
            bots += ["equity", "sim", "sim+eg"]
            if self.human:
                bots += ["sim+inf:human+eg"]
        return bots

    def reload(self, lexica: dict[str, dict]) -> None:
        """New leaves for some lexica: their games restart when next used."""
        changed = [
            n for n, lex in lexica.items() if self.lexica.get(n, {}).get("leaves") != lex["leaves"]
        ]
        self.lexica = lexica
        for name in changed:
            self.restart(name)

    def argv(self, name: str) -> list[str]:
        lex = self.lexica[name]
        argv = [self.bridge, "--lexicon", lex["path"], "--threads", str(self.threads)]
        if lex["leaves"]:
            argv += ["--leaves", lex["leaves"]]
        if self.human:
            argv += ["--human-params", self.human]
        return argv

    def session(self, sid: str) -> Session:
        """The visitor's session, made room for by closing idle or the oldest ones."""
        now = time.monotonic()
        with self.lock:
            for k, v in list(self.sessions.items()):
                if now - v.last > self.idle_s:
                    self._drop(k)
            if sid not in self.sessions:
                while len(self.sessions) >= self.max_sessions:
                    self._drop(min(self.sessions, key=lambda k: self.sessions[k].last))
                self.sessions[sid] = Session(self.default)
            s = self.sessions[sid]
            s.last = now
            return s

    def _drop(self, sid: str) -> None:
        s = self.sessions.pop(sid)
        if s.bridge:
            s.bridge.stop()

    def restart(self, lexicon: str) -> None:
        """Stop every game on this lexicon (its files changed); they restart when used."""
        with self.lock:
            for s in self.sessions.values():
                if s.lexicon == lexicon and s.bridge:
                    s.bridge.stop()
                    s.bridge = None

    def ask(self, sid: str, line: str, lexicon: str | None = None) -> dict:
        s = self.session(sid)
        if lexicon is not None and lexicon not in self.lexica:
            raise ValueError(f"unknown lexicon: {lexicon}")
        if not self.busy.acquire(timeout=30):
            return {"ok": False, "error": "the server is busy, try again in a moment"}
        try:
            if lexicon and lexicon != s.lexicon:
                if s.bridge:
                    s.bridge.stop()
                s.bridge, s.lexicon = None, lexicon
            if s.bridge is None:
                s.bridge = Bridge(self.argv(s.lexicon))
            reply = s.bridge.ask(line)
        finally:
            self.busy.release()
        if reply.get("state"):
            reply["state"]["lexicon"] = s.lexicon
        return reply

    def info(self, sid: str) -> dict:
        return {
            "ok": True,
            "current": self.session(sid).lexicon,
            "lexica": [
                {"name": n, "leaves": bool(v["leaves"]), "bots": self.bots(n)}
                for n, v in self.lexica.items()
            ],
        }

    def close(self) -> None:
        with self.lock:
            for k in list(self.sessions):
                self._drop(k)


LEVELS = ("fast", "normal", "max")
USE_FILE = ROOT / "lexica/leaves_use.json"


def is_loopback(host: str) -> bool:
    if host == "localhost":
        return True
    try:
        return ipaddress.ip_address(host).is_loopback
    except ValueError:
        return False


class LeavesTrainer:
    """One scripts/train_leaves.sh run at a time, for the Settings page. Local only:
    a public server must not let visitors keep its CPU busy for hours."""

    def __init__(self, engines: Engines, lexica_dir: Path, explicit: list[str], enabled: bool):
        self.engines, self.lexica_dir, self.explicit = engines, lexica_dir, explicit
        self.enabled = enabled
        self.lock = threading.Lock()
        self.proc: subprocess.Popen[str] | None = None
        self.job: dict = {
            "state": "idle",
            "lexicon": None,
            "level": None,
            "pct": 0,
            "eta_s": None,
            "message": "",
            "result": None,
        }
        try:
            self.use: dict[str, str] = json.loads(USE_FILE.read_text())
        except (OSError, ValueError):
            self.use = {}

    def refresh(self) -> None:
        lexica = find_lexica(self.lexica_dir, self.explicit, self.use)
        if lexica:
            self.engines.reload(lexica)

    def status(self) -> dict:
        with self.lock:
            job = dict(self.job)
        return {
            "ok": True,
            "enabled": self.enabled,
            "lexica": [
                {
                    "name": n,
                    "leaves": v.get("kind", "default" if v["leaves"] else "none"),
                    "has_own": v.get("has_own", False),
                }
                for n, v in self.engines.lexica.items()
            ],
            "job": job,
        }

    def set_use(self, lexicon: str, which: str) -> None:
        if lexicon not in self.engines.lexica or which not in ("own", "default"):
            raise ValueError("bad lexicon or choice")
        self.use[lexicon] = which
        USE_FILE.parent.mkdir(parents=True, exist_ok=True)
        USE_FILE.write_text(json.dumps(self.use, indent=1, sort_keys=True))
        self.refresh()

    def train(self, lexicon: str, level: str) -> None:
        if lexicon not in self.engines.lexica or level not in LEVELS:
            raise ValueError("bad lexicon or level")
        with self.lock:
            if self.proc and self.proc.poll() is None:
                raise ValueError("a training run is already going")
            env = dict(os.environ)
            env.setdefault("KINGBBLE_MAX_THREADS", "2")
            log_dir = ROOT / "build/leaves"
            log_dir.mkdir(parents=True, exist_ok=True)
            with open(log_dir / f"{lexicon}-{level}.log", "a") as log:
                self.proc = subprocess.Popen(
                    [
                        str(ROOT / "scripts/train_leaves.sh"),
                        self.engines.lexica[lexicon]["path"],
                        level,
                    ],
                    stdout=subprocess.PIPE,
                    stderr=log,
                    text=True,
                    bufsize=1,
                    env=env,
                    start_new_session=True,
                )
            proc = self.proc
            self.job = {
                "state": "running",
                "lexicon": lexicon,
                "level": level,
                "pct": 0,
                "eta_s": None,
                "message": "starting",
                "result": None,
            }
        threading.Thread(target=self._follow, args=(proc, lexicon), daemon=True).start()

    def cancel(self) -> None:
        with self.lock:
            proc = self.proc
            if not proc or proc.poll() is not None:
                raise ValueError("no training run is going")
            self.job["state"] = "cancelled"
            self.job["message"] = "cancelled: the next run at this level goes on from here"
        os.killpg(proc.pid, signal.SIGTERM)

    def _follow(self, proc: subprocess.Popen[str], lexicon: str) -> None:
        assert proc.stdout
        error = ""
        for line in proc.stdout:
            kind, _, rest = line.strip().partition(" ")
            with self.lock:
                if self.job["state"] != "running":
                    continue
                if kind == "@progress":
                    pct, eta, msg = (rest.split(" ", 2) + ["", "", ""])[:3]
                    self.job.update(
                        pct=float(pct), eta_s=None if eta == "-" else int(eta), message=msg
                    )
                elif kind == "@result":
                    win, lo, hi, games, verdict = rest.split()
                    self.job["result"] = {
                        "win_pct": float(win),
                        "ci_lo": float(lo),
                        "ci_hi": float(hi),
                        "games": int(games),
                        "verdict": verdict,
                    }
                elif kind == "@error":
                    error = rest
        code = proc.wait()
        with self.lock:
            if self.job["state"] != "running":
                return
            if code == 0:
                self.job.update(state="done", pct=100.0, eta_s=0, message="done")
                result = self.job["result"]
                adopt = result is None or result["verdict"] != "worse"
            else:
                self.job.update(state="failed", eta_s=None, message=error or f"exit code {code}")
                adopt = False
        if adopt:
            self.set_use(lexicon, "own")
        else:
            self.refresh()


def make_handler(engines: Engines, trainer: LeavesTrainer):
    class Handler(SimpleHTTPRequestHandler):
        def __init__(self, *a, **kw):
            super().__init__(*a, directory=str(STATIC), **kw)

        def log_message(self, fmt, *args):  # quiet
            pass

        def _sid(self) -> str:
            """The visitor's id from its cookie, or a new one sent back with the reply."""
            cookie = SimpleCookie(self.headers.get("Cookie", ""))
            sid = cookie["kb_sid"].value if "kb_sid" in cookie else ""
            if len(sid) < 16 or not sid.replace("-", "").replace("_", "").isalnum():
                sid = secrets.token_urlsafe(18)
                self._new_sid = sid
            return sid

        def _json(self, obj: dict, code: int = 200) -> None:
            data = json.dumps(obj).encode()
            self.send_response(code)
            if getattr(self, "_new_sid", None):
                self.send_header(
                    "Set-Cookie", f"kb_sid={self._new_sid}; Path=/; HttpOnly; SameSite=Strict"
                )
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):
            if self.path == "/api/info":
                return self._json(engines.info(self._sid()))
            if self.path == "/api/leaves":
                return self._json(trainer.status())
            return super().do_GET()

        def _leaves_post(self, body: dict) -> dict:
            if not (trainer.enabled and is_loopback(self.client_address[0])):
                return {"ok": False, "error": "leave training is only available on this computer"}
            if self.path == "/api/leaves/train":
                trainer.train(str(body.get("lexicon", "")), str(body.get("level", "")))
            elif self.path == "/api/leaves/cancel":
                trainer.cancel()
            else:
                trainer.set_use(str(body.get("lexicon", "")), str(body.get("which", "")))
            return {"ok": True}

        def do_POST(self):
            leaves = self.path in ("/api/leaves/train", "/api/leaves/cancel", "/api/leaves/use")
            if self.path != "/api" and not leaves:
                return self._json({"ok": False, "error": "not found"}, 404)
            try:
                length = int(self.headers.get("Content-Length", 0))
                body = json.loads(self.rfile.read(length) or b"{}")
                if leaves:
                    return self._json(self._leaves_post(body))
                line = command_line(body)
                lexicon = body.get("lexicon") if body.get("cmd") == "new" else None
                return self._json(engines.ask(self._sid(), line, lexicon))
            except (ValueError, RuntimeError, OSError) as e:
                return self._json({"ok": False, "error": str(e)})

    return Handler


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    p.add_argument(
        "--lexicon",
        action="append",
        default=[],
        help="a word list (.txt or .kwg); leaves beside it are found by name",
    )
    p.add_argument(
        "--lexica",
        default=str(ROOT / "lexica"),
        help="directory searched for more word lists (default: lexica/)",
    )
    p.add_argument("--human-params")
    p.add_argument("--threads", type=int, default=2)
    p.add_argument("--port", type=int, default=8765)
    p.add_argument("--host", default="127.0.0.1")
    exe = "kingbble_bridge.exe" if os.name == "nt" else "kingbble_bridge"
    p.add_argument("--bridge", default=str(ROOT / "build/engine" / exe))
    p.add_argument("--max-sessions", type=int, default=8, help="games open at once (default 8)")
    p.add_argument("--idle-minutes", type=float, default=30, help="close a game unused this long")
    p.add_argument(
        "--max-busy", type=int, default=0, help="commands at once (default CPUs/threads)"
    )
    a = p.parse_args()

    if not Path(a.bridge).exists():
        sys.exit(f"{a.bridge} not found: cmake --build build --target kingbble_bridge")
    use: dict[str, str] = {}
    try:
        use = json.loads(USE_FILE.read_text())
    except (OSError, ValueError):
        pass
    lexica = find_lexica(Path(a.lexica), a.lexicon, use)
    if not lexica:  # development fallback: the small free list of the tests
        lexica = find_lexica(ROOT / "lexica/fixtures", [])
        lexica = {k: v for k, v in lexica.items() if k == "enable_small"}
        lexica["enable_small"]["leaves"] = str(ROOT / "lexica/fixtures/leaves_small.klv2")
    engines = Engines(
        a.bridge, lexica, a.human_params, a.threads, a.max_sessions, a.idle_minutes * 60, a.max_busy
    )
    engines.ask("check", "state")  # fail early if the engine does not start
    engines.close()
    trainer = LeavesTrainer(engines, Path(a.lexica), a.lexicon, is_loopback(a.host))
    server = ThreadingHTTPServer((a.host, a.port), make_handler(engines, trainer))
    port = server.server_address[1]  # the real one with --port 0
    print(f"Kingbble: http://{a.host}:{port}  (lexica: {', '.join(lexica)})", flush=True)
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))  # stop the engines on kill too
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        engines.close()


if __name__ == "__main__":
    main()
