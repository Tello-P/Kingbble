"""Local web interface for Kingbble: serves ui/static and forwards /api calls to
the engine bridge (build/engine/kingbble_bridge), one game per server.

    python ui/server.py [--lexica lexica/] [--lexicon FILE ...]
        [--human-params models/human_model.txt] [--threads 2] [--port 8765]

Every word list in lexica/ (.txt or .kwg) can be chosen in the page; its leaves are
the file with the same name and .klv2, .klv or .csv. Then open http://localhost:8765.
Only the standard library is used.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import threading
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


def command_line(body: dict) -> str:
    cmd = str(body.get("cmd", ""))
    if cmd not in COMMANDS:
        raise ValueError(f"unknown command: {cmd}")
    args = [str(a) for a in body.get("args", [])]
    for a in args:
        if not a or any(c.isspace() for c in a):
            raise ValueError("bad argument")
    return " ".join([cmd, *args])


def find_lexica(directory: Path, explicit: list[str]) -> dict[str, dict]:
    """Word lists (.txt or .kwg) by name, each with its leaves file if one sits beside it."""
    out: dict[str, dict] = {}
    paths = [Path(p) for p in explicit]
    if directory.is_dir():
        paths += sorted(p for p in directory.iterdir() if p.suffix in (".txt", ".kwg"))
    for path in paths:
        if not path.is_file() or path.stem in out or "freq" in path.stem:
            continue
        leaves = next(
            (q for ext in (".klv2", ".klv", ".csv") if (q := path.with_suffix(ext)).is_file()), None
        )
        out[path.stem] = {"path": str(path), "leaves": str(leaves) if leaves else None}
    return out


class Engines:
    """One bridge per lexicon, started when first used."""

    def __init__(self, bridge: str, lexica: dict[str, dict], human: str | None, threads: int):
        self.bridge, self.lexica, self.human, self.threads = bridge, lexica, human, threads
        self.running: dict[str, Bridge] = {}
        self.current = next(iter(lexica))

    def bots(self, name: str) -> list[str]:
        bots = ["greedy"]
        if self.lexica[name]["leaves"]:
            bots += ["equity", "sim", "sim+eg"]
            if self.human:
                bots += ["sim+inf:human+eg"]
        return bots

    def get(self, name: str | None = None) -> Bridge:
        name = name or self.current
        if name not in self.lexica:
            raise ValueError(f"unknown lexicon: {name}")
        if name not in self.running:
            lex = self.lexica[name]
            argv = [self.bridge, "--lexicon", lex["path"], "--threads", str(self.threads)]
            if lex["leaves"]:
                argv += ["--leaves", lex["leaves"]]
            if self.human:
                argv += ["--human-params", self.human]
            self.running[name] = Bridge(argv)
        self.current = name
        return self.running[name]

    def info(self) -> dict:
        return {
            "ok": True,
            "current": self.current,
            "lexica": [
                {"name": n, "leaves": bool(v["leaves"]), "bots": self.bots(n)}
                for n, v in self.lexica.items()
            ],
        }


def make_handler(engines: Engines):
    class Handler(SimpleHTTPRequestHandler):
        def __init__(self, *a, **kw):
            super().__init__(*a, directory=str(STATIC), **kw)

        def log_message(self, fmt, *args):  # quiet
            pass

        def _json(self, obj: dict, code: int = 200) -> None:
            data = json.dumps(obj).encode()
            self.send_response(code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self):
            if self.path == "/api/info":
                return self._json(engines.info())
            return super().do_GET()

        def do_POST(self):
            if self.path != "/api":
                return self._json({"ok": False, "error": "not found"}, 404)
            try:
                length = int(self.headers.get("Content-Length", 0))
                body = json.loads(self.rfile.read(length) or b"{}")
                line = command_line(body)
                lexicon = body.get("lexicon") if body.get("cmd") == "new" else None
                reply = engines.get(lexicon).ask(line)
                if reply.get("state"):
                    reply["state"]["lexicon"] = engines.current
                return self._json(reply)
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
    p.add_argument("--bridge", default=str(ROOT / "build/engine/kingbble_bridge"))
    a = p.parse_args()

    if not Path(a.bridge).exists():
        sys.exit(f"{a.bridge} not found: cmake --build build --target kingbble_bridge")
    lexica = find_lexica(Path(a.lexica), a.lexicon)
    if not lexica:  # development fallback: the small free list of the tests
        lexica = find_lexica(ROOT / "lexica/fixtures", [])
        lexica = {k: v for k, v in lexica.items() if k == "enable_small"}
        lexica["enable_small"]["leaves"] = str(ROOT / "lexica/fixtures/leaves_small.klv2")
    engines = Engines(a.bridge, lexica, a.human_params, a.threads)
    engines.get()
    server = ThreadingHTTPServer((a.host, a.port), make_handler(engines))
    port = server.server_address[1]  # the real one with --port 0
    print(f"Kingbble: http://{a.host}:{port}  (lexica: {', '.join(lexica)})", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
