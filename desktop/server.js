// The desktop app's local service, in Node so the app needs no Python: it serves
// the interface (ui/static) and forwards /api calls to the engine bridge, one
// game at a time. It speaks the same protocol as ui/server.py, listens only on
// 127.0.0.1 and leaves training off (that needs the research tools).

"use strict";

const fs = require("fs");
const http = require("http");
const path = require("path");
const { spawn } = require("child_process");

// The Extreme level: simulation with the exact pre-endgame and the endgame solver,
// built only from parts that need no human data. The human-model bot is offered
// too when a model is given with --human-params (from source only).
const EXTREME = "sim+peg+eg";
// Commands the page may send; anything else is refused before reaching the engine.
const COMMANDS = new Set([
  "new", "state", "play", "check", "exchange", "pass", "bot", "hint", "undo", "view", "rewind", "gcg",
]);
const TYPES = {
  ".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8",
  ".css": "text/css; charset=utf-8", ".svg": "image/svg+xml", ".png": "image/png",
  ".ico": "image/x-icon", ".json": "application/json", ".woff2": "font/woff2",
  ".woff": "font/woff", ".ttf": "font/ttf", ".mp3": "audio/mpeg", ".ogg": "audio/ogg",
  ".wav": "audio/wav", ".webm": "video/webm", ".txt": "text/plain; charset=utf-8",
};

function commandLine(body) {
  const cmd = String(body.cmd ?? "");
  if (!COMMANDS.has(cmd)) throw new Error(`unknown command: ${cmd}`);
  const args = (Array.isArray(body.args) ? body.args : []).map(String);
  for (const a of args) if (!a || /\s/.test(a)) throw new Error("bad argument");
  return [cmd, ...args].join(" ");
}

// Word lists (.txt or .kwg) in a directory, each with its leaves beside it
// (same name, .klv2, .klv or .csv).
function findLexica(dir) {
  const out = {};
  let names = [];
  try {
    names = fs.readdirSync(dir).sort();
  } catch {
    return out;
  }
  for (const name of names) {
    const ext = path.extname(name);
    const stem = path.basename(name, ext);
    if (![".txt", ".kwg"].includes(ext) || stem in out || stem.includes("freq") || stem.endsWith(".own")) continue;
    const leaves = [".klv2", ".klv", ".csv"].map((e) => path.join(dir, stem + e)).find((p) => fs.existsSync(p));
    out[stem] = { path: path.join(dir, name), leaves: leaves || null };
  }
  return out;
}

// The engine process: one JSON line back per command line.
class Bridge {
  constructor(argv) {
    this.argv = argv;
    this.queue = Promise.resolve();
    this.start();
  }

  start() {
    this.buf = "";
    this.waiting = [];
    this.dead = false;
    this.proc = spawn(this.argv[0], this.argv.slice(1), { stdio: ["pipe", "pipe", "inherit"], windowsHide: true });
    this.proc.stdout.setEncoding("utf8");
    this.proc.stdout.on("data", (d) => {
      this.buf += d;
      let i;
      while ((i = this.buf.indexOf("\n")) >= 0) {
        const line = this.buf.slice(0, i);
        this.buf = this.buf.slice(i + 1);
        const w = this.waiting.shift();
        if (w) w.resolve(line);
      }
    });
    const fail = (e) => {
      this.dead = true;
      for (const w of this.waiting.splice(0)) w.reject(e);
    };
    this.proc.on("error", fail);
    this.proc.on("exit", () => fail(new Error("the engine stopped")));
    this.ready = this._line().then((l) => JSON.parse(l));
  }

  _line() {
    return new Promise((resolve, reject) => {
      if (this.dead) return reject(new Error("the engine stopped"));
      this.waiting.push({ resolve, reject });
    });
  }

  ask(line) {
    const run = async () => {
      if (this.dead) {
        this.start();
        await this.ready;
        return { ok: false, error: "the engine restarted, start a new game" };
      }
      await this.ready;
      const reply = this._line();
      this.proc.stdin.write(line + "\n");
      return JSON.parse(await reply);
    };
    const p = this.queue.then(run, run);
    this.queue = p.catch(() => {});
    return p;
  }

  stop() {
    if (!this.dead) {
      this.dead = true;
      this.proc.kill();
    }
  }
}

function bridgeName() {
  return process.platform === "win32" ? "kingbble_bridge.exe" : "kingbble_bridge";
}

// opts: { staticDir, bridge, lexicaDir, threads, human }
function createServer(opts) {
  const lexica = findLexica(opts.lexicaDir);
  const names = Object.keys(lexica);
  if (!names.length) throw new Error(`no word list in ${opts.lexicaDir}`);
  if (!fs.existsSync(opts.bridge)) throw new Error(`${opts.bridge} not found`);
  const preferred = names.find((n) => n.toUpperCase() === "ENABLE") || names[0];
  let current = preferred;
  let bridge = null;

  const bots = (name) => {
    const b = ["greedy"];
    if (lexica[name].leaves) {
      b.push("equity", "sim", "sim+eg", EXTREME);
      if (opts.human) b.push("sim+inf:human+eg");
    }
    return b;
  };
  const argv = (name) => {
    const lex = lexica[name];
    const a = [opts.bridge, "--lexicon", lex.path, "--threads", String(opts.threads || 2)];
    if (lex.leaves) a.push("--leaves", lex.leaves);
    if (opts.human) a.push("--human-params", opts.human);
    return a;
  };
  const info = () => ({
    ok: true,
    current,
    lexica: names.map((n) => ({ name: n, leaves: Boolean(lexica[n].leaves), bots: bots(n) })),
  });
  const leavesStatus = () => ({
    ok: true,
    enabled: false,
    lexica: names.map((n) => ({ name: n, leaves: lexica[n].leaves ? "default" : "none", has_own: false })),
    job: { state: "idle", lexicon: null, level: null, pct: 0, eta_s: null, message: "", result: null },
  });

  async function ask(line, lexicon) {
    if (lexicon != null && !(lexicon in lexica)) throw new Error(`unknown lexicon: ${lexicon}`);
    if (lexicon && lexicon !== current) {
      if (bridge) bridge.stop();
      bridge = null;
      current = lexicon;
    }
    if (!bridge) bridge = new Bridge(argv(current));
    const reply = await bridge.ask(line);
    if (reply.state) reply.state.lexicon = current;
    return reply;
  }

  const send = (res, code, body, type) => {
    res.writeHead(code, { "Content-Type": type, "Content-Length": Buffer.byteLength(body), "Cache-Control": "no-store" });
    res.end(body);
  };
  const json = (res, obj, code = 200) => send(res, code, JSON.stringify(obj), "application/json");

  const root = path.resolve(opts.staticDir);
  function serveStatic(req, res) {
    let rel;
    try {
      rel = decodeURIComponent(new URL(req.url, "http://x").pathname);
    } catch {
      return send(res, 400, "bad request", "text/plain");
    }
    if (rel.endsWith("/")) rel += "index.html";
    const file = path.resolve(root, "." + rel);
    if (file !== root && !file.startsWith(root + path.sep)) return send(res, 403, "forbidden", "text/plain");
    fs.readFile(file, (err, data) => {
      if (err) return send(res, 404, "not found", "text/plain");
      res.writeHead(200, { "Content-Type": TYPES[path.extname(file).toLowerCase()] || "application/octet-stream" });
      res.end(data);
    });
  }

  const server = http.createServer((req, res) => {
    const url = req.url.split("?")[0];
    if (req.method === "GET" && url === "/api/info") return json(res, info());
    if (req.method === "GET" && url === "/api/leaves") return json(res, leavesStatus());
    if (req.method === "POST" && url.startsWith("/api/leaves/"))
      return json(res, { ok: false, error: "leave training is not available in the desktop app" });
    if (req.method === "POST" && url === "/api") {
      let data = "";
      req.setEncoding("utf8");
      req.on("data", (d) => {
        data += d;
        if (data.length > 1 << 20) req.destroy();
      });
      req.on("end", async () => {
        try {
          const body = JSON.parse(data || "{}");
          const line = commandLine(body);
          const lexicon = body.cmd === "new" ? body.lexicon ?? null : null;
          json(res, await ask(line, lexicon));
        } catch (e) {
          json(res, { ok: false, error: String(e.message || e) });
        }
      });
      return;
    }
    if (req.method === "GET" || req.method === "HEAD") return serveStatic(req, res);
    json(res, { ok: false, error: "not found" }, 404);
  });

  return {
    server,
    // Starts the engine once to fail early, then listens on a free local port.
    async listen(port = 0) {
      const check = await ask("state", null);
      if (!check || typeof check.ok !== "boolean") throw new Error("the engine did not answer");
      await new Promise((resolve, reject) => {
        server.once("error", reject);
        server.listen(port, "127.0.0.1", resolve);
      });
      return `http://127.0.0.1:${server.address().port}`;
    },
    close() {
      if (bridge) bridge.stop();
      bridge = null;
      server.close();
    },
  };
}

module.exports = { createServer, findLexica, commandLine, bridgeName, EXTREME };
