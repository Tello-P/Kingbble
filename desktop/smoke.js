// Checks the staged app without a window: starts the local service on the staged
// bridge and word list, then plays a few moves through the HTTP API.
//
//   node stage.js && node smoke.js

"use strict";

const path = require("path");
const { createServer, bridgeName } = require("./server");

(async () => {
  const staging = path.join(__dirname, "staging");
  const service = createServer({
    staticDir: path.join(__dirname, "..", "ui", "static"),
    bridge: path.join(staging, "engine", bridgeName()),
    lexicaDir: path.join(staging, "lexica"),
    threads: 2,
  });
  const url = await service.listen(0);
  const post = (body) => fetch(url + "/api", { method: "POST", body: JSON.stringify(body) }).then((r) => r.json());
  const fail = (msg) => {
    service.close();
    console.error("smoke test failed: " + msg);
    process.exit(1);
  };

  const info = await (await fetch(url + "/api/info")).json();
  const enable = info.lexica.find((l) => l.name === "ENABLE");
  if (!enable || !enable.leaves || !enable.bots.includes("sim+eg")) fail("ENABLE or its leaves missing: " + JSON.stringify(info));
  if ((await fetch(url + "/")).status !== 200) fail("the page does not load");

  let r = await post({ cmd: "new", args: ["11", "sim+eg", "equity", "1", "0"], lexicon: "ENABLE" });
  if (!r.ok) fail("new game: " + r.error);
  for (let i = 0; i < 6; i++) {
    r = await post({ cmd: "bot" });
    if (!r.ok) fail("bot move: " + r.error);
  }
  const moves = r.state.history.length;
  if (moves !== 6) fail(`expected 6 moves, got ${moves}`);
  if ((await post({ cmd: "quit" })).ok !== false) fail("a command outside the list reached the engine");
  console.log(`smoke test passed: ${moves} moves, scores ${r.state.scores.join("-")}`);
  service.close();
})().catch((e) => {
  console.error(e);
  process.exit(1);
});
