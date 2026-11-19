// The engine in a worker: loads the WebAssembly module, the word list and the
// leaves, then answers one bridge command per message ({id, line} -> {id, reply}).
importScripts("engine.js");

let engine = null;

async function fetchBytes(url, label) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`cannot load ${url}`);
  postMessage({ progress: label });
  return new Uint8Array(await res.arrayBuffer());
}

const ready = (async () => {
  const [mod, words, leaves] = await Promise.all([
    KingbbleEngine(),
    fetchBytes("ENABLE.txt", "words"),
    fetchBytes("ENABLE-leaves.txt", "leaves"),
  ]);
  mod.FS.writeFile("/ENABLE.txt", words);
  mod.FS.writeFile("/ENABLE.csv", leaves);
  const reply = mod.ccall("kb_init", "string", ["string", "string"], ["/ENABLE.txt", "/ENABLE.csv"]);
  engine = mod;
  return JSON.parse(reply);
})();

ready.then(
  (info) => postMessage({ ready: info }),
  (e) => postMessage({ ready: { ok: false, error: String(e && e.message ? e.message : e) } }),
);

onmessage = async (e) => {
  const { id, line } = e.data;
  let reply;
  try {
    await ready;
    reply = JSON.parse(engine.ccall("kb_command", "string", ["string"], [line]));
  } catch (err) {
    reply = { ok: false, error: "the engine stopped, reload the page" };
  }
  postMessage({ id, reply });
};
