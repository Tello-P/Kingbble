// Gathers what the packaged app carries besides its code, into desktop/staging/:
//   engine/   the bridge built by CMake (kingbble_bridge or kingbble_bridge.exe)
//   lexica/   ENABLE.txt (public domain, checked by SHA-256) and ENABLE.csv, the
//             leave table Kingbble trained from scratch (configs/enable_leaves_gen10)
//   legal/    LICENSE and THIRD_PARTY_NOTICES.md
//
//   node stage.js [--build ../build] [--enable FILE_OR_URL]
//
// Uses only Node's standard library, so it runs the same on Linux and Windows.

"use strict";

const crypto = require("crypto");
const fs = require("fs");
const https = require("https");
const path = require("path");
const zlib = require("zlib");

const ROOT = path.resolve(__dirname, "..");
const OUT = path.join(__dirname, "staging");
const ENABLE_URL = "https://raw.githubusercontent.com/dolph/dictionary/master/enable1.txt";
// SHA-256 of ENABLE.txt after normalizing (uppercase, A-Z, 2-15 letters, sorted),
// the same file scripts/fetch_lexicon.py writes and the CI checks.
const ENABLE_SHA = "0d6d47327b05142362f52844113497d0439bd0a7fc8d0bc33ee40ecff0216ac1";

function arg(name, fallback) {
  const i = process.argv.indexOf(name);
  return i >= 0 && i + 1 < process.argv.length ? process.argv[i + 1] : fallback;
}

function download(url, redirects = 5) {
  return new Promise((resolve, reject) => {
    https.get(url, (res) => {
      if ([301, 302, 303, 307, 308].includes(res.statusCode) && res.headers.location && redirects > 0) {
        res.resume();
        return resolve(download(new URL(res.headers.location, url).toString(), redirects - 1));
      }
      if (res.statusCode !== 200) {
        res.resume();
        return reject(new Error(`${url}: HTTP ${res.statusCode}`));
      }
      const chunks = [];
      res.on("data", (c) => chunks.push(c));
      res.on("end", () => resolve(Buffer.concat(chunks)));
    }).on("error", reject);
  });
}

function normalize(text) {
  const words = new Set();
  for (const line of text.split(/\r?\n/)) {
    const w = (line.trim().split(/\s+/)[0] || "").toUpperCase();
    if (/^[A-Z]{2,15}$/.test(w)) words.add(w);
  }
  return [...words].sort().join("\n") + "\n";
}

async function main() {
  const build = path.resolve(arg("--build", path.join(ROOT, "build")));
  const exe = process.platform === "win32" ? "kingbble_bridge.exe" : "kingbble_bridge";
  const bridge = [path.join(build, "engine", exe), path.join(build, "engine", "Release", exe)].find((p) =>
    fs.existsSync(p),
  );
  if (!bridge) throw new Error(`${exe} not found under ${build}: cmake --build build --target kingbble_bridge`);

  fs.rmSync(OUT, { recursive: true, force: true });
  for (const d of ["engine", "lexica", "legal"]) fs.mkdirSync(path.join(OUT, d), { recursive: true });
  fs.copyFileSync(bridge, path.join(OUT, "engine", exe));
  if (process.platform !== "win32") fs.chmodSync(path.join(OUT, "engine", exe), 0o755);

  const source = arg("--enable", ENABLE_URL);
  const raw = /^https?:/.test(source) ? await download(source) : fs.readFileSync(source);
  const enable = normalize(raw.toString("utf8"));
  const sha = crypto.createHash("sha256").update(enable).digest("hex");
  if (sha !== ENABLE_SHA) throw new Error(`ENABLE.txt SHA-256 ${sha}, expected ${ENABLE_SHA}`);
  fs.writeFileSync(path.join(OUT, "lexica", "ENABLE.txt"), enable);

  const leaves = zlib.gunzipSync(fs.readFileSync(path.join(ROOT, "configs", "enable_leaves_gen10.csv.gz")));
  fs.writeFileSync(path.join(OUT, "lexica", "ENABLE.csv"), leaves);

  fs.copyFileSync(path.join(ROOT, "LICENSE"), path.join(OUT, "legal", "LICENSE.txt"));
  fs.copyFileSync(path.join(ROOT, "THIRD_PARTY_NOTICES.md"), path.join(OUT, "legal", "THIRD_PARTY_NOTICES.md"));
  console.log(`staged ${bridge}, ENABLE (${enable.split("\n").length - 1} words) and its leaves in ${OUT}`);
}

main().catch((e) => {
  console.error(e.message || e);
  process.exit(1);
});
