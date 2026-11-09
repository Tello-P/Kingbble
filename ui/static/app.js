"use strict";
// Kingbble interface. The game logic lives in the engine (kingbble_bridge); this
// page shows the state it sends and turns clicks, drags and keys into commands.

const $ = (id) => document.getElementById(id);
const N = 15;
const COLS = "ABCDEFGHIJKLMNO";

// ---------- texts ----------

const TEXT = {
  en: {
    newGame: "New game", exportGcg: "Download game (GCG)", settings: "Settings", play: "Play",
    recall: "Recall", shuffle: "Shuffle", exchange: "Exchange", confirmExchange: "Exchange selected",
    cancel: "Cancel", pass: "Pass", undo: "Undo", pause: "Pause", resume: "Resume", step: "Next move",
    speed: "Speed", unseen: "Unseen", analysis: "Analysis", bestMoves: "Best moves", racks: "Racks",
    moves: "Moves", modeVs: "You vs Kingbble", modeWatch: "Bot vs bot", modeAnalysis: "Analysis",
    helpVs: "Play a game against the engine.", helpWatch: "Watch two bots play each other.",
    helpAnalysis: "Two players on one screen, every rack visible, with the engine's suggestions.",
    firstMove: "First move", you: "You", random: "Random", lexicon: "Lexicon", seed: "Seed",
    start: "Start", language: "Language", theme: "Appearance", auto: "Automatic", light: "Light",
    dark: "Dark", input: "Placing tiles", inputBoth: "Drag, click and type", inputDrag: "Drag only",
    inputType: "Click and type only", assists: "Assists", showHints: "Best moves panel",
    showPreview: "Score preview", showUnseen: "Unseen tiles", showCoords: "Coordinates",
    extras: "Extras", sound: "Sound", clock: "Game clock", off: "Off", intro: "Intro animation",
    done: "Done", blankTitle: "Which letter is the blank?", seeBoard: "See board", again: "Play again",
    opponent: "Opponent", bot: "Bot", player: "Player", strength: "Thinking time", instant: "instant",
    yourTurn: "Your turn", turnOf: "{0} to move", thinking: "{0} is thinking", paused: "Paused",
    over: "Game over", youWin: "You win!", botWins: "Kingbble wins", wins: "{0} wins", tie: "Tie",
    finalText: "{0} – {1}", finalRacks: "Tiles left: {0}.", points: "{0} pts", bag: "bag {0}",
    passes: "pass", exchanges: "exchange {0}", yourRack: "Your rack", rackOf: "{0}'s rack",
    pickExchange: "Tap the tiles you want to exchange.", noTile: "You don't have {0}.",
    noServer: "Can't reach the Kingbble server.", noLeaves: "(no leaves file: simple bots only)",
    timeOver: "out of time",
    helpBoth: "Drag tiles, or click a square and type. Click it again to change direction. Enter plays, Esc recalls.",
    helpDrag: "Drag tiles onto the board. Tap a placed tile to take it back.",
    helpType: "Click a square and type. Click it again to change direction. Enter plays, Esc recalls.",
    bots: {
      greedy: "Easy · highest score", equity: "Medium · static equity", sim: "Strong · simulation",
      "sim+eg": "Very strong · simulation + endgame",
      "sim+inf:human+eg": "Maximum · rack inference + endgame",
    },
    errors: [
      [/^no game/, "No game in progress."], [/game is over/, "The game is over."],
      [/bot's turn/, "It's the bot's turn."], [/your turn/, "It's your turn."],
      [/not on your rack: (.)/, "You don't have {1}."], [/square already taken/, "That square is taken."],
      [/one row or column/, "Tiles must be in one row or column."],
      [/centre square/, "The first move must cover the centre square."],
      [/not a valid move/, "Not a valid move: gaps, not connected, or a word not in the lexicon."],
      [/at least 7 tiles/, "You can only exchange with 7 or more tiles in the bag."],
      [/nothing to undo/, "Nothing to undo."], [/no tiles/, "Place some tiles first."],
      [/engine restarted/, "The engine restarted. Start a new game."],
    ],
  },
  es: {
    newGame: "Nueva partida", exportGcg: "Descargar partida (GCG)", settings: "Ajustes", play: "Jugar",
    recall: "Recoger", shuffle: "Mezclar", exchange: "Cambiar", confirmExchange: "Cambiar selección",
    cancel: "Cancelar", pass: "Pasar", undo: "Deshacer", pause: "Pausa", resume: "Seguir",
    step: "Siguiente jugada", speed: "Velocidad", unseen: "Sin ver", analysis: "Análisis",
    bestMoves: "Mejores jugadas", racks: "Atriles", moves: "Jugadas", modeVs: "Tú vs Kingbble",
    modeWatch: "Bot vs bot", modeAnalysis: "Análisis", helpVs: "Juega una partida contra el motor.",
    helpWatch: "Mira cómo juegan dos bots entre sí.",
    helpAnalysis: "Dos jugadores en una pantalla, con todos los atriles a la vista y las sugerencias del motor.",
    firstMove: "Empieza", you: "Tú", random: "Al azar", lexicon: "Lexicón", seed: "Semilla",
    start: "Empezar", language: "Idioma", theme: "Apariencia", auto: "Automática", light: "Clara",
    dark: "Oscura", input: "Colocar fichas", inputBoth: "Arrastrar, clic y escribir",
    inputDrag: "Solo arrastrar", inputType: "Solo clic y escribir", assists: "Ayudas",
    showHints: "Panel de mejores jugadas", showPreview: "Puntos antes de jugar",
    showUnseen: "Fichas sin ver", showCoords: "Coordenadas", extras: "Extras", sound: "Sonido",
    clock: "Reloj de partida", off: "No", intro: "Animación de entrada", done: "Hecho",
    blankTitle: "¿Qué letra es el comodín?", seeBoard: "Ver tablero", again: "Otra partida",
    opponent: "Rival", bot: "Bot", player: "Jugador", strength: "Tiempo de reflexión",
    instant: "instantáneo", yourTurn: "Tu turno", turnOf: "Turno de {0}", thinking: "{0} está pensando",
    paused: "En pausa", over: "Partida terminada", youWin: "¡Has ganado!", botWins: "Gana Kingbble",
    wins: "Gana {0}", tie: "Empate", finalText: "{0} – {1}", finalRacks: "Fichas sobrantes: {0}.",
    points: "{0} ptos", bag: "bolsa {0}", passes: "pasa", exchanges: "cambia {0}",
    yourRack: "Tu atril", rackOf: "Atril de {0}", pickExchange: "Toca las fichas que quieres cambiar.",
    noTile: "No tienes la {0}.", noServer: "No hay conexión con el servidor de Kingbble.",
    noLeaves: "(sin fichero de leaves: solo bots sencillos)", timeOver: "sin tiempo",
    helpBoth: "Arrastra fichas, o haz clic en una casilla y escribe. Otro clic cambia la dirección. Intro juega, Esc recoge.",
    helpDrag: "Arrastra fichas al tablero. Toca una ficha puesta para devolverla.",
    helpType: "Haz clic en una casilla y escribe. Otro clic cambia la dirección. Intro juega, Esc recoge.",
    bots: {
      greedy: "Fácil · máxima puntuación", equity: "Medio · equity estática", sim: "Fuerte · simulación",
      "sim+eg": "Muy fuerte · simulación + finales",
      "sim+inf:human+eg": "Máximo · inferencia del atril + finales",
    },
    errors: [
      [/^no game/, "No hay partida en curso."], [/game is over/, "La partida ha terminado."],
      [/bot's turn/, "Le toca al bot."], [/your turn/, "Te toca a ti."],
      [/not on your rack: (.)/, "No tienes la {1}."], [/square already taken/, "Esa casilla está ocupada."],
      [/one row or column/, "Las fichas deben ir en una sola fila o columna."],
      [/centre square/, "La primera jugada debe pasar por la casilla central."],
      [/not a valid move/, "Jugada no válida: huecos, no conecta o alguna palabra no está en el lexicón."],
      [/at least 7 tiles/, "Solo puedes cambiar con 7 o más fichas en la bolsa."],
      [/nothing to undo/, "No hay nada que deshacer."], [/no tiles/, "Pon alguna ficha primero."],
      [/engine restarted/, "El motor se reinició. Empieza una partida nueva."],
    ],
  },
};
const BONUS = { T: "tw", D: "dw", t: "tl", d: "dl" };
const BONUS_LABEL = { en: { T: "TW", D: "DW", t: "TL", d: "DL" }, es: { T: "TP", D: "DP", t: "TL", d: "DL" } };

function t(k, ...a) {
  const s = (TEXT[settings.lang] || TEXT.en)[k] ?? TEXT.en[k] ?? k;
  return typeof s === "string" ? s.replace(/\{(\d)\}/g, (_, i) => a[+i] ?? "") : s;
}

function tr(msg) {
  for (const [re, text] of t("errors")) {
    const m = msg.match(re);
    if (m) return text.replace("{1}", m[1] || "");
  }
  return msg;
}

// ---------- settings ----------

const DEFAULTS = {
  lang: (navigator.language || "en").startsWith("es") ? "es" : "en",
  theme: "auto", input: "both", hints: true, preview: true, unseen: true, coords: true,
  sound: true, clock: "0", intro: true,
  // last new-game choices
  mode: "vs", bots: ["sim+eg", "equity"], secs: [5, 5], first: "0", lexicon: "",
};
let settings = { ...DEFAULTS };
try {
  Object.assign(settings, JSON.parse(localStorage.getItem("kingbble-settings") || "{}"));
} catch {}
function saveSettings() {
  try {
    localStorage.setItem("kingbble-settings", JSON.stringify(settings));
  } catch {}
}

// ---------- sound (synthesised, no files) ----------

let audio = null;
function tone(freq, dur = 0.08, type = "sine", gain = 0.06, delay = 0) {
  if (!settings.sound) return;
  try {
    audio = audio || new (window.AudioContext || window.webkitAudioContext)();
    const o = audio.createOscillator(), g = audio.createGain();
    const t0 = audio.currentTime + delay;
    o.type = type;
    o.frequency.value = freq;
    g.gain.setValueAtTime(gain, t0);
    g.gain.exponentialRampToValueAtTime(0.0001, t0 + dur);
    o.connect(g).connect(audio.destination);
    o.start(t0);
    o.stop(t0 + dur);
  } catch {}
}
const sfx = {
  place: () => tone(880, 0.05, "triangle", 0.05),
  lift: () => tone(660, 0.04, "triangle", 0.03),
  play: () => { tone(660, 0.12); tone(990, 0.16, "sine", 0.06, 0.08); },
  bot: () => { tone(520, 0.12); tone(780, 0.16, "sine", 0.05, 0.08); },
  bingo: () => [523, 659, 784, 1047].forEach((f, i) => tone(f, 0.18, "sine", 0.06, i * 0.08)),
  error: () => tone(160, 0.18, "square", 0.04),
  end: () => [784, 659, 523].forEach((f, i) => tone(f, 0.25, "sine", 0.06, i * 0.12)),
};

// ---------- state ----------

let S = null;            // state from the engine
let info = null;         // lexica and bots offered by the server
let rack = [];           // [{id, ch}] in display order; ch "A".."Z" or "?"
let rackSeat = -1;
let nextId = 1;
let pending = new Map(); // "r,c" -> {r, c, id, letter, blank}
let order = [];
let cursor = null;       // {r, c, dir}
let selected = null;
let exchange = null;     // Set of picked rack ids
let busy = false;
let paused = false;
let gameToken = 0;
let checkTimer = 0;
let clocks = [0, 0];     // ms left per player
let clockTick = 0;
let animate = new Set(); // squares to animate on next render

const isHuman = (p) => S && S.players[p] === "human";
const humans = () => (S ? [0, 1].filter(isHuman) : []);
const mode = () => (!S ? "vs" : humans().length === 2 ? "analysis" : humans().length === 1 ? "vs" : "watch");
const myTurn = () => S && !S.over && isHuman(S.to_move);
const key = (r, c) => `${r},${c}`;
const boardAt = (r, c) => (S ? S.board[r][c] : ".");
const isFree = (r, c) => r >= 0 && c >= 0 && r < N && c < N && boardAt(r, c) === "." && !pending.has(key(r, c));
const usedIds = () => new Set([...pending.values()].map((p) => p.id));

function name(p) {
  if (!S) return "";
  const h = humans();
  if (isHuman(p)) return h.length === 1 ? t("you") : `${t("player")} ${p + 1}`;
  return h.length === 0 ? `Kingbble ${p + 1}` : "Kingbble";
}

// ---------- engine ----------

async function api(cmd, args = [], extra = {}) {
  const res = await fetch("/api", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ cmd, args, ...extra }),
  });
  return res.json();
}

function toast(text) {
  const el = $("toast");
  el.textContent = text;
  el.classList.add("show");
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => el.classList.remove("show"), 3200);
}

async function run(cmd, args = [], extra = {}) {
  if (busy) return null;
  busy = true;
  updateControls();
  try {
    const r = await api(cmd, args, extra);
    if (!r.ok) {
      sfx.error();
      toast(tr(r.error));
      return null;
    }
    if (r.state) setState(r.state);
    return r;
  } catch {
    toast(t("noServer"));
    return null;
  } finally {
    busy = false;
    updateControls();
  }
}

// Lets the bots move while it is their turn (with a pause between moves when watching).
async function botLoop() {
  const token = gameToken;
  while (S && !S.over && !isHuman(S.to_move) && token === gameToken) {
    if (mode() === "watch") {
      if (paused) return;
      await new Promise((ok) => setTimeout(ok, 3000 - +$("speed").value));
      if (paused || token !== gameToken) return;
    }
    setStatus(t("thinking", name(S.to_move)), true);
    const r = await run("bot");
    if (!r || token !== gameToken) return;
    const last = S.history[S.history.length - 1];
    if (last && last.move.placed && last.move.placed.length === 7) sfx.bingo();
    else sfx.bot();
  }
  renderPanel();
}

// ---------- state updates ----------

function setState(st) {
  const prev = S;
  S = st;
  const moved = !prev || prev.history.length !== st.history.length || prev.seed !== st.seed;
  if (moved) {
    pending.clear();
    order = [];
    exchange = null;
    $("hints").innerHTML = "";
    const last = st.history[st.history.length - 1];
    animate = new Set(prev && last && last.move.placed ? last.move.placed.map(([r, c]) => key(r, c)) : []);
  }
  if (prev && prev.to_move !== st.to_move) startClock();
  syncRack();
  render();
  if (st.over && (!prev || !prev.over)) showOver();
}

// The rack shown: the human to move, else the human in the game, else the bot to move.
function shownSeat() {
  if (!S) return -1;
  if (isHuman(S.to_move)) return S.to_move;
  const h = humans();
  return h.length ? h[0] : S.to_move;
}

function syncRack() {
  const seat = shownSeat();
  const str = (S.racks[seat] || "").split("");
  const keep = [];
  const left = [...str];
  if (seat === rackSeat)
    for (const tile of rack) {
      const i = left.indexOf(tile.ch);
      if (i >= 0) {
        keep.push(tile);
        left.splice(i, 1);
      }
    }
  for (const ch of left) keep.push({ id: nextId++, ch });
  rack = keep;
  rackSeat = seat;
  if (selected && !rack.some((x) => x.id === selected)) selected = null;
}

// ---------- clocks ----------

function resetClocks() {
  const ms = +settings.clock * 60000;
  clocks = [ms, ms];
  startClock();
}

function startClock() {
  clearInterval(clockTick);
  if (!+settings.clock || !S || S.over) return renderClocks();
  let last = performance.now();
  clockTick = setInterval(() => {
    const now = performance.now();
    if (!(mode() === "watch" && paused) && S && !S.over) clocks[S.to_move] -= now - last;
    last = now;
    renderClocks();
  }, 250);
}

function fmt(ms) {
  const neg = ms < 0;
  const s = Math.floor(Math.abs(ms) / 1000);
  return `${neg ? "−" : ""}${Math.floor(s / 60)}:${String(s % 60).padStart(2, "0")}`;
}

function renderClocks() {
  for (const p of [0, 1]) {
    const el = $(`p${p}`).querySelector(".clock");
    el.textContent = +settings.clock ? fmt(clocks[p]) : "";
    el.classList.toggle("low", +settings.clock > 0 && clocks[p] < 60000);
  }
}

// ---------- rendering ----------

function render() {
  renderBoard();
  renderRack();
  renderPanel();
  updateControls();
  schedulePreview();
}

function tileEl(letter, blank, extra = "") {
  const el = document.createElement("div");
  el.className = `tile ${blank ? "blank" : ""} ${extra}`;
  const up = letter.toUpperCase();
  const v = blank || letter === "?" ? "" : (S && S.values[up]) || "";
  el.innerHTML = `${letter === "?" ? "" : up}<span class="v">${v}</span>`;
  return el;
}

function renderBoard() {
  const b = $("board");
  b.innerHTML = "";
  const last = new Set();
  const lh = S && S.history[S.history.length - 1];
  if (lh && lh.move.placed) for (const [r, c] of lh.move.placed) last.add(key(r, c));
  const labels = BONUS_LABEL[settings.lang] || BONUS_LABEL.en;
  for (let r = 0; r < N; r++) {
    for (let c = 0; c < N; c++) {
      const cell = document.createElement("div");
      cell.className = "cell";
      cell.dataset.r = r;
      cell.dataset.c = c;
      const k = key(r, c);
      const ch = boardAt(r, c);
      const p = pending.get(k);
      if (ch !== ".") {
        const te = tileEl(ch, ch !== ch.toUpperCase(), last.has(k) ? "last" : "");
        if (animate.has(k)) te.classList.add("pop");
        cell.appendChild(te);
      } else if (p) {
        const te = tileEl(p.letter, p.blank, "pending");
        te.dataset.pending = k;
        cell.appendChild(te);
      } else if (r === 7 && c === 7) {
        cell.classList.add("center");
      } else {
        const bonus = S ? S.bonus[r][c] : ".";
        if (BONUS[bonus]) {
          cell.classList.add(BONUS[bonus]);
          cell.textContent = labels[bonus];
        }
      }
      if (cursor && cursor.r === r && cursor.c === c) cell.classList.add("cursor", cursor.dir);
      b.appendChild(cell);
    }
  }
  animate.clear();
  $("board-wrap").classList.toggle("coords-on", settings.coords);
}

function renderRack() {
  const el = $("rack");
  el.innerHTML = "";
  el.classList.toggle("exchange", !!exchange);
  el.classList.toggle("readonly", !myTurn() || rackSeat !== (S && S.to_move));
  const used = usedIds();
  for (const tile of rack) {
    const te = tileEl(tile.ch, false);
    te.dataset.id = tile.id;
    if (used.has(tile.id)) te.classList.add("used");
    if (selected === tile.id) te.classList.add("selected");
    if (exchange && exchange.has(tile.id)) te.classList.add("picked");
    el.appendChild(te);
  }
  for (let i = rack.length; i < 7; i++) el.appendChild(Object.assign(document.createElement("div"), { className: "slot" }));
  $("rack-label").textContent =
    !S ? "" : humans().length === 1 && isHuman(rackSeat) ? t("yourRack") : t("rackOf", name(rackSeat));
}

function renderPanel() {
  if (!S) return;
  for (const p of [0, 1]) {
    const el = $(`p${p}`);
    el.querySelector(".name").textContent = name(p);
    el.querySelector(".sub").textContent = isHuman(p) ? "" : (t("bots")[S.players[p]] || S.players[p]);
    el.querySelector(".score").textContent = S.scores[p];
    el.classList.toggle("turn", !S.over && S.to_move === p);
  }
  renderClocks();
  if (S.over) setStatus(t("over"));
  else if (mode() === "watch" && paused) setStatus(t("paused"));
  else if (isHuman(S.to_move)) setStatus(humans().length === 1 ? t("yourTurn") : t("turnOf", name(S.to_move)));

  $("card-unseen").hidden = !settings.unseen;
  const total = Object.values(S.unseen).reduce((a, b) => a + b, 0);
  $("unseen-count").textContent = total;
  $("bag-count").textContent = t("bag", S.bag);
  const un = $("unseen");
  un.innerHTML = "";
  for (const [l, n] of Object.entries(S.unseen)) {
    const s = document.createElement("span");
    s.innerHTML = `<b>${l}</b><i>${n}</i>`;
    un.appendChild(s);
  }

  $("card-hints").hidden = !settings.hints || mode() === "watch";
  $("card-racks").hidden = mode() !== "watch";
  if (mode() === "watch")
    $("racks").innerHTML = [0, 1].map((p) => `<div><span>${name(p)}</span><span class="mv">${S.racks[p] || ""}</span></div>`).join("");

  const h = $("history");
  h.innerHTML = "";
  for (const e of S.history) {
    const li = document.createElement("li");
    li.className = `p${e.player}`;
    const m = e.move;
    const text = m.kind === "place" ? m.text.replace(/ \(\d+\)$/, "") : m.kind === "exchange" ? t("exchanges", m.count) : t("passes");
    const score = m.kind === "place" ? ` <span class="rk">+${m.score}</span>` : "";
    const rk = e.rack ? `<span class="rk">${e.rack}</span>` : "";
    li.innerHTML = `<span><span class="who">${name(e.player)}</span><span class="mv">${text}</span>${score} ${rk}</span><span class="cum">${e.cumulative}</span>`;
    h.appendChild(li);
  }
  if (S.over) {
    const li = document.createElement("li");
    li.className = "end";
    li.innerHTML = `<span>${t("over")}</span><span class="cum">${S.scores[0]} – ${S.scores[1]}</span>`;
    h.appendChild(li);
  }
  h.scrollTop = h.scrollHeight;
}

function setStatus(text, thinking = false) {
  const s = $("status");
  s.textContent = text;
  s.classList.toggle("thinking", thinking);
}

function updateControls() {
  const watch = mode() === "watch";
  $("controls").hidden = watch;
  $("watch-controls").hidden = !watch;
  const turn = myTurn() && !busy;
  $("btn-play").disabled = !turn || (exchange ? exchange.size === 0 : pending.size === 0);
  $("btn-play").textContent = exchange ? t("confirmExchange") : t("play");
  $("btn-recall").disabled = !exchange && pending.size === 0;
  $("btn-recall").textContent = exchange ? t("cancel") : t("recall");
  $("btn-exchange").disabled = !turn || !!exchange || !S || S.bag < 7;
  $("btn-pass").disabled = !turn;
  $("btn-undo").disabled = busy || !S || !S.history.some((e) => isHuman(e.player));
  $("btn-hint").disabled = busy || !myTurn();
  $("btn-shuffle").disabled = !S;
  $("btn-pause").textContent = paused ? t("resume") : t("pause");
  $("btn-step").disabled = !paused || busy || !S || S.over;
  $("help").textContent = watch ? "" : t({ both: "helpBoth", drag: "helpDrag", type: "helpType" }[settings.input]);
}

// ---------- preview ----------

function placementArgs() {
  return [...pending.values()].map((p) => `${p.r},${p.c},${p.blank ? p.letter.toLowerCase() : p.letter}`);
}

function schedulePreview() {
  clearTimeout(checkTimer);
  const pv = $("preview");
  if (!pending.size || !myTurn() || !settings.preview) {
    pv.textContent = "";
    pv.className = "preview";
    return;
  }
  const args = placementArgs();
  checkTimer = setTimeout(async () => {
    const r = await api("check", args).catch(() => null);
    if (!r || placementArgs().join() !== args.join()) return;
    pv.className = `preview ${r.ok ? "ok" : "bad"}`;
    pv.textContent = r.ok ? `${r.move.text.replace(/ \(\d+\)$/, "")} · ${t("points", r.score)}` : tr(r.error);
  }, 120);
}

// ---------- placing tiles ----------

function askBlank() {
  return new Promise((resolve) => {
    const d = $("dlg-blank");
    const box = $("blank-letters");
    box.innerHTML = "";
    let chosen = null;
    for (const l of "ABCDEFGHIJKLMNOPQRSTUVWXYZ") {
      const b = document.createElement("button");
      b.type = "button";
      b.textContent = l;
      b.onclick = () => {
        chosen = l;
        d.close();
      };
      box.appendChild(b);
    }
    d.onclose = () => resolve(chosen);
    d.showModal();
  });
}

async function place(id, r, c, letter = null) {
  if (!myTurn() || !isFree(r, c) || rackSeat !== S.to_move) return false;
  const tile = rack.find((x) => x.id === id);
  if (!tile || usedIds().has(id)) return false;
  let blank = false;
  if (tile.ch === "?") {
    blank = true;
    letter = letter || (await askBlank());
    if (!letter) return false;
  } else letter = tile.ch;
  pending.set(key(r, c), { r, c, id, letter, blank });
  order.push(key(r, c));
  selected = null;
  sfx.place();
  render();
  return true;
}

function unplace(k) {
  pending.delete(k);
  order = order.filter((x) => x !== k);
}

function recall() {
  pending.clear();
  order = [];
  exchange = null;
  render();
}

function advance(r, c, dir) {
  do {
    if (dir === "h") c++;
    else r++;
  } while (r < N && c < N && boardAt(r, c) !== ".");
  return r < N && c < N ? { r, c, dir } : null;
}

// ---------- drag and click ----------

let drag = null;
const canDrag = () => settings.input !== "type";
const canType = () => settings.input !== "drag";

function onPointerDown(e) {
  const tile = e.target.closest(".tile");
  if (!tile || e.button > 0 || document.querySelector("dialog[open]")) return;
  const fromRack = tile.dataset.id && !tile.classList.contains("used") && tile.closest("#rack");
  const fromBoard = tile.dataset.pending;
  if (!fromRack && !fromBoard) return;
  drag = { el: tile, id: fromRack ? +tile.dataset.id : null, pkey: fromBoard || null, x: e.clientX, y: e.clientY, ghost: null };
}

function onPointerMove(e) {
  if (!drag) return;
  if (!drag.ghost && canDrag() && Math.hypot(e.clientX - drag.x, e.clientY - drag.y) > 6) {
    const p = drag.pkey && pending.get(drag.pkey);
    const ch = p ? p.letter : rack.find((x) => x.id === drag.id).ch;
    drag.ghost = tileEl(ch, p && p.blank, "ghost-tile");
    document.body.appendChild(drag.ghost);
    drag.el.style.opacity = ".3";
    sfx.lift();
  }
  if (drag.ghost) {
    drag.ghost.style.left = `${e.clientX}px`;
    drag.ghost.style.top = `${e.clientY}px`;
    document.querySelectorAll(".drop").forEach((x) => x.classList.remove("drop"));
    const target = dropTarget(e);
    if (target) target.classList.add("drop");
  }
}

function dropTarget(e) {
  const under = document.elementFromPoint(e.clientX, e.clientY);
  if (!under) return null;
  const cell = under.closest(".cell");
  if (cell && isFree(+cell.dataset.r, +cell.dataset.c)) return cell;
  return under.closest(".rack");
}

async function onPointerUp(e) {
  if (!drag) return;
  const d = drag;
  drag = null;
  document.querySelectorAll(".drop").forEach((x) => x.classList.remove("drop"));
  if (!d.ghost) return tapTile(d);
  d.ghost.remove();
  d.el.style.opacity = "";
  const target = dropTarget(e);
  const p = d.pkey && pending.get(d.pkey);
  const id = p ? p.id : d.id;
  if (target && target.classList.contains("cell")) {
    if (p) unplace(d.pkey);
    if (!(await place(id, +target.dataset.r, +target.dataset.c, p && p.blank ? p.letter : null))) render();
  } else {
    if (p) unplace(d.pkey);
    else if (target) reorder(id, e.clientX);
    render();
  }
}

function reorder(id, x) {
  const tiles = [...$("rack").querySelectorAll(".tile")];
  let at = tiles.findIndex((el) => {
    const b = el.getBoundingClientRect();
    return x < b.left + b.width / 2;
  });
  if (at < 0) at = rack.length;
  const from = rack.findIndex((x2) => x2.id === id);
  const [tile] = rack.splice(from, 1);
  if (at > from) at--;
  rack.splice(at, 0, tile);
}

function tapTile(d) {
  if (d.pkey) {
    unplace(d.pkey);
    sfx.lift();
  } else if (exchange) {
    exchange.has(d.id) ? exchange.delete(d.id) : exchange.add(d.id);
  } else if (myTurn()) {
    selected = selected === d.id ? null : d.id;
  }
  render();
}

async function onBoardClick(e) {
  const cell = e.target.closest(".cell");
  if (!cell || e.target.closest(".tile.pending") || !myTurn()) return;
  const r = +cell.dataset.r, c = +cell.dataset.c;
  if (!isFree(r, c)) return;
  if (selected) {
    await place(selected, r, c);
    return;
  }
  if (!canType()) return;
  cursor = cursor && cursor.r === r && cursor.c === c ? { r, c, dir: cursor.dir === "h" ? "v" : "h" } : { r, c, dir: cursor ? cursor.dir : "h" };
  renderBoard();
}

// ---------- keyboard ----------

async function onKey(e) {
  if (document.querySelector("dialog[open]") || e.target.matches("input, select, textarea")) return;
  if (mode() === "watch") {
    if (e.key === " ") {
      togglePause();
      e.preventDefault();
    }
    return;
  }
  if (e.key === "Enter") return void play();
  if (e.key === "Escape") return void recall();
  if (!canType()) return;
  if (e.key === "Backspace") {
    const k = order[order.length - 1];
    if (!k) return;
    const p = pending.get(k);
    unplace(k);
    cursor = { r: p.r, c: p.c, dir: cursor ? cursor.dir : "h" };
    render();
    e.preventDefault();
    return;
  }
  const arrows = { ArrowLeft: [0, -1], ArrowRight: [0, 1], ArrowUp: [-1, 0], ArrowDown: [1, 0] };
  if (arrows[e.key] && cursor) {
    const [dr, dc] = arrows[e.key];
    cursor = { r: Math.min(N - 1, Math.max(0, cursor.r + dr)), c: Math.min(N - 1, Math.max(0, cursor.c + dc)), dir: dc ? "h" : "v" };
    renderBoard();
    e.preventDefault();
    return;
  }
  if (!/^[a-z]$/i.test(e.key) || !cursor || !myTurn() || e.ctrlKey || e.metaKey || e.altKey) return;
  const L = e.key.toUpperCase();
  const used = usedIds();
  let tile = !e.shiftKey && rack.find((x) => x.ch === L && !used.has(x.id));
  let blank = false;
  if (!tile) {
    tile = rack.find((x) => x.ch === "?" && !used.has(x.id));
    blank = true;
  }
  if (!tile) return toast(t("noTile", L));
  let { r, c, dir } = cursor;
  if (!isFree(r, c)) {
    const n = advance(r, c, dir);
    if (!n) return;
    ({ r, c } = n);
  }
  if (await place(tile.id, r, c, blank ? L : null)) {
    cursor = advance(r, c, dir) || cursor;
    renderBoard();
  }
}

// ---------- actions ----------

async function afterHumanMove(r) {
  if (!r) return;
  cursor = null;
  const last = S.history[S.history.length - 1];
  if (last && last.move.placed && last.move.placed.length === 7) sfx.bingo();
  else sfx.play();
  await botLoop();
}

async function play() {
  if (!myTurn() || busy) return;
  if (exchange) {
    if (!exchange.size) return;
    const tiles = rack.filter((x) => exchange.has(x.id)).map((x) => x.ch).join("");
    exchange = null;
    const r = await run("exchange", [tiles]);
    if (!r) render();
    return afterHumanMove(r);
  }
  if (!pending.size) return;
  await afterHumanMove(await run("play", placementArgs()));
}

async function hint() {
  const r = await run("hint", [10]);
  if (!r) return;
  const ol = $("hints");
  ol.innerHTML = "";
  for (const h of r.hints) {
    const li = document.createElement("li");
    const m = h.move;
    const text = m.kind === "place" ? m.text : m.kind === "exchange" ? `${t("exchange")} ${m.exchanged}` : t("pass");
    li.innerHTML = `<span class="mv">${text}</span><span class="eq">${h.equity.toFixed(1)}</span>`;
    li.onclick = () => loadHint(m);
    ol.appendChild(li);
  }
}

function loadHint(m) {
  pending.clear();
  order = [];
  exchange = null;
  if (m.kind === "exchange") {
    exchange = new Set();
    for (const ch of m.exchanged) {
      const tile = rack.find((x) => x.ch === ch && !exchange.has(x.id));
      if (tile) exchange.add(tile.id);
    }
  } else if (m.kind === "place") {
    for (const [r, c, l] of m.placed) {
      const blank = l !== l.toUpperCase();
      const used = usedIds();
      const tile = rack.find((x) => x.ch === (blank ? "?" : l) && !used.has(x.id));
      if (!tile) continue;
      pending.set(key(r, c), { r, c, id: tile.id, letter: l.toUpperCase(), blank });
      order.push(key(r, c));
    }
  }
  render();
}

function shuffle() {
  for (let i = rack.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [rack[i], rack[j]] = [rack[j], rack[i]];
  }
  renderRack();
}

function togglePause() {
  paused = !paused;
  updateControls();
  renderPanel();
  if (!paused) botLoop();
}

async function step() {
  if (!paused || busy || !S || S.over) return;
  setStatus(t("thinking", name(S.to_move)), true);
  if (await run("bot")) sfx.bot();
  renderPanel();
}

// ---------- new game ----------

let dlgMode = settings.mode;

function lexiconInfo(nameL) {
  return info.lexica.find((l) => l.name === nameL) || info.lexica[0];
}

const STEPS = [0.5, 1, 2, 3, 5, 8, 10, 15, 20, 30, 45, 60];

function renderBotCfg() {
  const lex = lexiconInfo($("opt-lexicon").value);
  const box = $("bot-cfg");
  box.innerHTML = "";
  const seats = dlgMode === "vs" ? [0] : dlgMode === "watch" ? [0, 1] : [];
  for (const i of seats) {
    const div = document.createElement("div");
    div.className = "botcfg";
    const title = dlgMode === "vs" ? t("opponent") : `${t("bot")} ${i + 1}`;
    const opts = lex.bots.map((b) => `<option value="${b}">${t("bots")[b] || b}</option>`).join("");
    const secs = settings.secs[i] ?? 5;
    const idx = Math.max(0, STEPS.findIndex((s) => s >= secs));
    div.innerHTML = `<span class="title">${title}</span><select data-bot="${i}">${opts}</select>
      <label class="strength"><span>${t("strength")}</span><input type="range" min="0" max="${STEPS.length - 1}" value="${idx}" data-secs="${i}"><output></output></label>`;
    box.appendChild(div);
    const sel = div.querySelector("select");
    sel.value = lex.bots.includes(settings.bots[i]) ? settings.bots[i] : lex.bots[lex.bots.length > 3 ? 3 : lex.bots.length - 1];
    const range = div.querySelector("input");
    const out = div.querySelector("output");
    const sync = () => {
      const timed = sel.value.startsWith("sim");
      range.disabled = !timed;
      out.textContent = timed ? `${STEPS[+range.value]} s` : t("instant");
    };
    sel.onchange = sync;
    range.oninput = sync;
    sync();
  }
  $("first-row").hidden = dlgMode !== "vs";
  $("mode-help").textContent = t({ vs: "helpVs", watch: "helpWatch", analysis: "helpAnalysis" }[dlgMode]);
  for (const b of $("mode").querySelectorAll("button")) b.classList.toggle("on", b.dataset.mode === dlgMode);
}

function newGame() {
  if (!info) return;
  const lexSel = $("opt-lexicon");
  lexSel.innerHTML = info.lexica.map((l) => `<option value="${l.name}">${l.name}${l.leaves ? "" : " " + t("noLeaves")}</option>`).join("");
  lexSel.value = info.lexica.some((l) => l.name === settings.lexicon) ? settings.lexicon : info.current;
  lexSel.onchange = renderBotCfg;
  $("opt-first").value = settings.first;
  dlgMode = settings.mode;
  renderBotCfg();
  const d = $("dlg-new");
  d.returnValue = "";
  d.onclose = () => {
    if (d.returnValue === "ok") startGame();
  };
  d.showModal();
}

async function startGame() {
  const bots = [0, 1].map((i) => document.querySelector(`[data-bot="${i}"]`)?.value);
  const secs = [0, 1].map((i) => {
    const r = document.querySelector(`[data-secs="${i}"]`);
    return r ? STEPS[+r.value] : 5;
  });
  let players, seconds;
  if (dlgMode === "analysis") {
    players = ["human", "human"];
    seconds = [0, 0];
  } else if (dlgMode === "watch") {
    players = bots;
    seconds = secs.map((s, i) => (bots[i].startsWith("sim") ? s : 0));
  } else {
    const firstOpt = $("opt-first").value;
    const botFirst = firstOpt === "r" ? Math.random() < 0.5 : firstOpt === "1";
    const b = bots[0], s = b.startsWith("sim") ? secs[0] : 0;
    players = botFirst ? [b, "human"] : ["human", b];
    seconds = [s, s];
    settings.first = firstOpt;
  }
  settings.mode = dlgMode;
  if (dlgMode !== "analysis") {
    settings.bots = [bots[0], bots[1] ?? settings.bots[1]];
    settings.secs = [secs[0], dlgMode === "watch" ? secs[1] : settings.secs[1]];
  }
  settings.lexicon = $("opt-lexicon").value;
  saveSettings();
  const seed = +$("opt-seed").value || 1 + Math.floor(Math.random() * 2 ** 31);
  gameToken++;
  paused = false;
  cursor = null;
  exchange = null;
  rack = [];
  rackSeat = -1;
  const r = await run("new", [seed, players[0], players[1], seconds[0], seconds[1]], { lexicon: settings.lexicon });
  if (!r) return;
  resetClocks();
  render();
  botLoop();
}

function showOver() {
  clearInterval(clockTick);
  sfx.end();
  const [a, b] = S.scores;
  let title;
  if (a === b) title = t("tie");
  else {
    const w = a > b ? 0 : 1;
    title = humans().length === 1 ? (isHuman(w) ? t("youWin") : t("botWins")) : t("wins", name(w));
  }
  $("over-title").textContent = title;
  const left = [0, 1].filter((p) => S.racks[p]).map((p) => `${name(p)} ${S.racks[p]}`).join(", ");
  $("over-text").textContent = `${name(0)} ${a} – ${b} ${name(1)}. ${left ? t("finalRacks", left) : ""}`;
  const d = $("dlg-over");
  d.returnValue = "";
  d.onclose = () => d.returnValue === "new" && newGame();
  setTimeout(() => d.showModal(), 600);
}

async function downloadGcg() {
  const r = await run("gcg");
  if (!r) return;
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([r.gcg], { type: "text/plain" }));
  a.download = `kingbble-${S.seed}.gcg`;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
}

// ---------- settings dialog ----------

function applySettings() {
  const root = document.documentElement;
  if (settings.theme === "auto") delete root.dataset.theme;
  else root.dataset.theme = settings.theme;
  root.lang = settings.lang;
  for (const el of document.querySelectorAll("[data-i18n]")) el.textContent = t(el.dataset.i18n);
  for (const el of document.querySelectorAll("[data-i18n-title]")) el.title = t(el.dataset.i18nTitle);
  for (const el of document.querySelectorAll("[data-i18n-placeholder]")) el.placeholder = t(el.dataset.i18nPlaceholder);
  if (settings.input === "drag") cursor = null;
  render();
}

function openSettings() {
  const d = $("dlg-settings");
  for (const el of d.querySelectorAll("[data-set]")) {
    const k = el.dataset.set;
    if (el.type === "checkbox") el.checked = !!settings[k];
    else el.value = String(settings[k]);
    el.onchange = () => {
      const old = settings.clock;
      settings[k] = el.type === "checkbox" ? el.checked : el.value;
      saveSettings();
      applySettings();
      if (k === "clock" && old !== settings.clock) resetClocks();
    };
  }
  d.showModal();
}

// ---------- start ----------

function splash() {
  const el = $("splash");
  if (!settings.intro) return el.remove();
  const end = () => {
    el.classList.add("done");
    setTimeout(() => el.remove(), 600);
  };
  const timer = setTimeout(end, 3000);
  el.addEventListener("click", () => {
    clearTimeout(timer);
    end();
  }, { once: true });
}

async function init() {
  splash();
  $("coords-cols").innerHTML = [...COLS].map((c) => `<span>${c}</span>`).join("");
  $("coords-rows").innerHTML = Array.from({ length: N }, (_, i) => `<span>${i + 1}</span>`).join("");
  applySettings();
  document.addEventListener("pointerdown", onPointerDown);
  document.addEventListener("pointermove", onPointerMove);
  document.addEventListener("pointerup", onPointerUp);
  document.addEventListener("pointercancel", () => {
    if (drag && drag.ghost) drag.ghost.remove();
    if (drag) drag.el.style.opacity = "";
    drag = null;
  });
  $("board").addEventListener("click", onBoardClick);
  document.addEventListener("keydown", onKey);
  $("btn-play").onclick = play;
  $("btn-recall").onclick = recall;
  $("btn-shuffle").onclick = shuffle;
  $("btn-exchange").onclick = () => {
    pending.clear();
    order = [];
    exchange = new Set();
    render();
    toast(t("pickExchange"));
  };
  $("btn-pass").onclick = async () => {
    pending.clear();
    order = [];
    await afterHumanMove(await run("pass"));
  };
  $("btn-undo").onclick = async () => {
    gameToken++;
    await run("undo");
  };
  $("btn-hint").onclick = hint;
  $("btn-new").onclick = newGame;
  $("btn-gcg").onclick = downloadGcg;
  $("btn-settings").onclick = openSettings;
  $("btn-pause").onclick = togglePause;
  $("btn-step").onclick = step;
  for (const b of $("mode").querySelectorAll("button"))
    b.onclick = () => {
      dlgMode = b.dataset.mode;
      renderBotCfg();
    };
  try {
    info = await (await fetch("/api/info")).json();
  } catch {
    return toast(t("noServer"));
  }
  const r = await api("state").catch(() => null);
  if (r && r.ok) {
    setState(r.state);
    resetClocks();
    botLoop();
  } else setTimeout(newGame, settings.intro ? 2900 : 0);
}

init();
