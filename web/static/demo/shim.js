// The web demo has no server: this file answers the page's /api calls with the
// engine running in a worker (WebAssembly), and applies the limits of the demo:
// one word list, at most 5 seconds of thinking, and the Extreme level locked.
"use strict";
(() => {
  const LEXICON = "ENABLE";
  const BOTS = ["greedy", "equity", "sim", "sim+eg"];
  const LOCKED = "locked:extreme";
  const MAX_SECONDS = 5;
  const COMMANDS = new Set(["new", "state", "play", "check", "exchange", "pass", "bot", "hint", "undo", "view", "rewind", "gcg"]);
  const es = () => {
    try {
      const saved = JSON.parse(localStorage.getItem("kingbble-settings") || "{}");
      return (saved.lang || (navigator.language || "en").slice(0, 2)) === "es";
    } catch {
      return false;
    }
  };
  const TEXT = {
    locked: () => (es() ? "Extremo · inferencia del atril + finales (solo en la app)" : "Extreme · rack inference + endgames (app only)"),
    note: () => (es()
      ? "El nivel Extremo usa todos los núcleos del ordenador y no cabe en una pestaña del navegador. Está en la app de escritorio."
      : "The Extreme level uses every core of the computer and does not fit in a browser tab. It is in the desktop app."),
    get: () => (es() ? "Descargar Kingbble" : "Download Kingbble"),
    loading: () => (es() ? "Cargando el motor y las 168.551 palabras…" : "Loading the engine and its 168,551 words…"),
    failed: () => (es() ? "No se pudo cargar el motor. Recarga la página." : "The engine could not be loaded. Reload the page."),
    back: () => (es() ? "Web de Kingbble" : "Kingbble website"),
    demo: () => (es() ? "Demo web" : "Web demo"),
  };

  // The language chosen on the site wins over the saved one.
  const lang = new URLSearchParams(location.search).get("lang");
  if (lang === "en" || lang === "es") {
    try {
      const saved = JSON.parse(localStorage.getItem("kingbble-settings") || "{}");
      saved.lang = lang;
      localStorage.setItem("kingbble-settings", JSON.stringify(saved));
    } catch {}
  }
  const siteLang = () => (es() ? "es" : "en");

  const worker = new Worker("worker.js");
  const waiting = new Map();
  let nextId = 1;
  let resolveReady;
  const ready = new Promise((ok) => (resolveReady = ok));
  worker.onmessage = (e) => {
    const m = e.data;
    if (m.ready) {
      resolveReady(m.ready);
      const el = document.getElementById("demo-loading");
      if (el) m.ready.ok ? el.remove() : (el.textContent = TEXT.failed());
    } else if (m.id) {
      waiting.get(m.id)?.(m.reply);
      waiting.delete(m.id);
    }
  };
  worker.onerror = () => resolveReady({ ok: false, error: "worker" });
  const ask = (line) => new Promise((ok) => {
    const id = nextId++;
    waiting.set(id, ok);
    worker.postMessage({ id, line });
  });

  async function command(body) {
    const cmd = String(body.cmd || "");
    if (!COMMANDS.has(cmd)) return { ok: false, error: `unknown command: ${cmd}` };
    const args = (body.args || []).map(String);
    if (args.some((a) => !a || /\s/.test(a))) return { ok: false, error: "bad argument" };
    if (cmd === "new") {
      if (args.length !== 5) return { ok: false, error: "bad argument" };
      for (const kind of [args[1], args[2]])
        if (kind !== "human" && !BOTS.includes(kind)) return { ok: false, error: "this level is only in the desktop app" };
      for (const i of [3, 4]) args[i] = String(Math.min(MAX_SECONDS, Math.max(0, Number(args[i]) || 0)));
    }
    const info = await ready;
    if (!info.ok) return { ok: false, error: "the engine could not be loaded" };
    const reply = await ask([cmd, ...args].join(" "));
    if (reply.state) reply.state.lexicon = LEXICON;
    return reply;
  }

  const json = (data, status = 200) =>
    new Response(JSON.stringify(data), { status, headers: { "Content-Type": "application/json" } });
  const realFetch = window.fetch.bind(window);
  window.fetch = async (url, options = {}) => {
    const path = typeof url === "string" ? url : url.url;
    if (path === "/api/info") {
      const info = await ready;
      if (!info.ok) return json({ ok: false }, 500);
      return json({ ok: true, current: LEXICON, lexica: [{ name: LEXICON, leaves: !!info.leaves, bots: [...BOTS, LOCKED] }] });
    }
    if (path === "/api") return json(await command(JSON.parse(options.body || "{}")));
    if (path.startsWith("/api/leaves")) return json({ ok: false, enabled: false });
    return realFetch(url, options);
  };

  // The locked level: shown in the menu, cannot be chosen, with the way to get it.
  function lockLevels(root) {
    for (const opt of root.querySelectorAll(`option[value="${LOCKED}"]`)) {
      opt.disabled = true;
      opt.textContent = "\u{1F512} " + TEXT.locked();
      const cfg = opt.closest(".botcfg");
      if (cfg && !cfg.querySelector(".locked-note")) {
        const p = document.createElement("p");
        p.className = "locked-note";
        p.innerHTML = `<span></span> <a></a>`;
        p.firstElementChild.textContent = TEXT.note();
        const a = p.lastElementChild;
        a.textContent = TEXT.get() + " →";
        a.href = `../${siteLang()}/download.html`;
        cfg.appendChild(p);
      }
    }
  }

  document.addEventListener("DOMContentLoaded", () => {
    const cfg = document.getElementById("bot-cfg");
    if (cfg) new MutationObserver(() => lockLevels(cfg)).observe(cfg, { childList: true });
    const brand = document.querySelector(".brand");
    if (brand) {
      const a = document.createElement("a");
      a.className = "demo-back";
      a.href = `../${siteLang()}/index.html`;
      a.textContent = TEXT.demo();
      a.title = TEXT.back();
      brand.after(a);
      // The language can change in Settings.
      document.getElementById("dlg-settings")?.addEventListener("close", () => {
        a.href = `../${siteLang()}/index.html`;
        a.textContent = TEXT.demo();
        a.title = TEXT.back();
      });
    }
    const el = document.createElement("div");
    el.id = "demo-loading";
    el.setAttribute("role", "status");
    el.textContent = TEXT.loading();
    document.body.appendChild(el);
    ready.then((info) => (info.ok ? el.remove() : (el.textContent = TEXT.failed())));
  });
})();
