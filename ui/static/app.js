"use strict";
// Kingbble interface. The game logic lives in the engine (kingbble_bridge); this
// page shows the state it sends and turns clicks, drags and keys into commands.

const $ = (id) => document.getElementById(id);
const N = 15;
const COLS = "ABCDEFGHIJKLMNO";

// ---------- texts ----------

const TEXT = {
  en: {
    leaves: "Leaves", lexicon: "Word list", leavesNow: "In use", leavesUse: "Use", leavesOwn: "Own (trained here)",
    leavesDefault: "Included", leavesLevel: "Training", lvFast: "Fast (minutes)", lvNormal: "Normal (1-2 h)",
    lvMax: "Maximum (overnight)", leavesTrain: "Train leaves", lvNone: "none", lvOwnNow: "own", lvDefaultNow: "included",
    lvRunning: "Training {0}…", lvDone: "Done", lvFailed: "Failed", lvStopped: "Stopped (progress kept)",
    lvBetter: "better than the current ones", lvEqual: "as good as the current ones", lvWorse: "worse than the current ones",
    lvResult: "{0}% wins in {1} games ({2}–{3}%): {4}", lvEta: "{0} left",
    leavesNote: "Changing the leaves restarts the engine for this word list: a game in progress is lost.",
    enter: "Start", boardLabel: "Board", rackLabel: "Your rack", panelLabel: "Game panel", mode: "Game mode",
    boardHelp: "Arrow keys move the cursor, Space changes the direction, letters place tiles, Backspace takes the last one back, Enter plays.",
    cellTW: "triple word", cellDW: "double word", cellTL: "triple letter", cellDL: "double letter", cellCenter: "centre",
    cellEmpty: "empty", cellPending: "{0}, placed this turn", dirH: "across", dirV: "down", tilePts: "{0} points", tilePtsOne: "1 point",
    blankTile: "blank", tileUsed: "on the board", tilePicked: "chosen to exchange",
    newGame: "New game", exportGcg: "Download game (GCG)", settings: "Settings", play: "Play",
    recall: "Recall", shuffle: "Shuffle", exchange: "Exchange", confirmExchange: "Exchange selected",
    cancel: "Cancel", pass: "Pass", undo: "Undo", pause: "Pause", resume: "Resume", step: "Next move",
    speed: "Speed", unseen: "Unseen", tiles: "Tiles", rewindHere: "Play from here",
    backToGame: "Back to game", first: "First move", prev: "Previous move", next: "Next move",
    last: "Current position", movePos: "Move {0} of {1}", reviewingStatus: "Reviewing move {0} of {1}",
    reviewingText: "Board after move {0} of {1}.", scoring: "Scoring", squares: "Squares",
    howToScore: "How a move scores", tileValues: "Tile values",
    sqTW: "Triple word: the whole word counts three times.", sqDW: "Double word: the whole word counts twice.",
    sqTL: "Triple letter: that tile counts three times.", sqDL: "Double letter: that tile counts twice.",
    sqCenter: "Centre: the first move must cover it. It is a double word.",
    rules: [
      "Add up the values of every letter in the word. Blanks are worth 0.",
      "Letter bonuses (2L, 3L) apply first, then word bonuses (×2, ×3, gold squares) multiply the word. Two word bonuses multiply each other (×4, ×9).",
      "Bonuses count only for tiles placed this turn. Squares already covered count at face value.",
      "Every new word you form across counts too, with the bonuses under your new tiles.",
      "Playing all 7 tiles is a bingo: +50.",
      "Going out with the bag empty earns twice the tiles left on the opponent's rack. After six scoreless turns in a row, the game ends and each player loses the value of their own rack.",
    ],
    rulesEx: ["E.g. FAR = 4 + 1 + 1 = 6", "A on 2L: 1 × 2 = 2; then the word on ×3: × 3", "", "", "", ""],
    bingo: "bingo", points2: "{0} pts", analysisHelp: "Moves ranked by static equity.", analysis: "Analysis", bestMoves: "Best moves", racks: "Racks",
    moves: "Moves", modeVs: "You vs Kingbble", modeWatch: "Bot vs bot", modeAnalysis: "Analysis",
    helpVs: "Play a game against the engine.", helpWatch: "Watch two bots play each other.",
    helpAnalysis: "Two players on one screen, every rack visible, with the engine's suggestions.",
    firstMove: "First move", you: "You", random: "Random", lexicon: "Lexicon", seed: "Seed",
    start: "Start", language: "Language", theme: "Appearance", auto: "Automatic", light: "Light",
    dark: "Dark", input: "Placing tiles", inputBoth: "Drag, click and type", inputDrag: "Drag only",
    inputType: "Click and type only", assists: "Assists", showHints: "Best moves panel",
    showPreview: "Score preview", showUnseen: "Unseen tiles", showCoords: "Coordinates",
    extras: "Extras", sound: "Sound", clock: "Game clock", off: "Off", intro: "Intro animation", introSound: "Intro sound", isCalm: "Calm", isMusicbox: "Music box", isBreath: "Breath", isNone: "None", tryIt: "Play",
    about: "About", aboutVersion: "Kingbble 1.0, free software under the GNU GPL v3.",
    aboutCredits: "Word list: ENABLE (public domain). Leaves trained with wolges (MIT, Andy Kurnia). Desktop app: Electron (MIT). Fonts: IBM Plex Sans and Young Serif (SIL Open Font License).",
    aboutTrademark: "Kingbble is not affiliated with or endorsed by Hasbro or Mattel. Scrabble® is a trademark of its owners.",
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
    leaves: "Leaves", lexicon: "Lista de palabras", leavesNow: "En uso", leavesUse: "Usar", leavesOwn: "Propias (entrenadas aquí)",
    leavesDefault: "Incluidas", leavesLevel: "Entrenamiento", lvFast: "Rápido (minutos)", lvNormal: "Normal (1-2 h)",
    lvMax: "Máximo (de noche)", leavesTrain: "Entrenar leaves", lvNone: "ninguna", lvOwnNow: "propias", lvDefaultNow: "incluidas",
    lvRunning: "Entrenando {0}…", lvDone: "Hecho", lvFailed: "Falló", lvStopped: "Parado (se conserva el progreso)",
    lvBetter: "mejores que las actuales", lvEqual: "igual de buenas que las actuales", lvWorse: "peores que las actuales",
    lvResult: "{0}% de victorias en {1} partidas ({2}–{3}%): {4}", lvEta: "faltan {0}",
    leavesNote: "Cambiar las leaves reinicia el motor de esta lista: se pierde la partida en curso.",
    enter: "Iniciar", boardLabel: "Tablero", rackLabel: "Tu atril", panelLabel: "Panel de la partida", mode: "Modo de juego",
    boardHelp: "Las flechas mueven el cursor, Espacio cambia la dirección, las letras ponen fichas, Retroceso quita la última e Intro juega.",
    cellTW: "triple palabra", cellDW: "doble palabra", cellTL: "triple letra", cellDL: "doble letra", cellCenter: "centro",
    cellEmpty: "vacía", cellPending: "{0}, puesta este turno", dirH: "horizontal", dirV: "vertical", tilePts: "{0} puntos", tilePtsOne: "1 punto",
    blankTile: "comodín", tileUsed: "en el tablero", tilePicked: "elegida para cambiar",
    newGame: "Nueva partida", exportGcg: "Descargar partida (GCG)", settings: "Ajustes", play: "Jugar",
    recall: "Recoger", shuffle: "Mezclar", exchange: "Cambiar", confirmExchange: "Cambiar selección",
    cancel: "Cancelar", pass: "Pasar", undo: "Deshacer", pause: "Pausa", resume: "Seguir",
    step: "Siguiente jugada", speed: "Velocidad", unseen: "Sin ver", tiles: "Fichas",
    rewindHere: "Volver a esta jugada", backToGame: "Volver a la partida", first: "Primera jugada",
    prev: "Jugada anterior", next: "Jugada siguiente", last: "Posición actual", movePos: "Jugada {0} de {1}",
    reviewingStatus: "Repasando la jugada {0} de {1}", reviewingText: "Tablero tras la jugada {0} de {1}.",
    scoring: "Puntos", squares: "Casillas", howToScore: "Cómo se puntúa", tileValues: "Valor de las fichas",
    sqTW: "Triple palabra: la palabra entera vale el triple.", sqDW: "Doble palabra: la palabra entera vale el doble.",
    sqTL: "Triple letra: esa ficha vale el triple.", sqDL: "Doble letra: esa ficha vale el doble.",
    sqCenter: "Centro: la primera jugada debe pasar por aquí. Es doble palabra.",
    rules: [
      "Suma el valor de cada letra de la palabra. El comodín vale 0.",
      "Primero se aplican los bonus de letra (2L, 3L) y luego los de palabra (×2, ×3, casillas doradas) multiplican la palabra. Dos bonus de palabra se multiplican entre sí (×4, ×9).",
      "Los bonus solo cuentan para las fichas que pones en ese turno. Las casillas ya ocupadas valen lo que la ficha.",
      "Cada palabra nueva que formes en perpendicular también suma, con los bonus de tus fichas nuevas.",
      "Poner las 7 fichas del atril es un bingo: +50.",
      "Si acabas tus fichas con la bolsa vacía, sumas el doble de lo que le queda al rival. Tras seis turnos seguidos sin puntos, la partida termina y cada uno resta lo que tiene en el atril.",
    ],
    rulesEx: ["Ej.: FAR = 4 + 1 + 1 = 6", "A en 2L: 1 × 2 = 2; luego la palabra en ×3: × 3", "", "", "", ""],
    bingo: "bingo", points2: "{0} ptos",
    analysisHelp: "Jugadas ordenadas por equity estática.", analysis: "Análisis",
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
    clock: "Reloj de partida", off: "No", intro: "Animación de entrada", introSound: "Sonido de entrada", isCalm: "Sereno", isMusicbox: "Caja de música", isBreath: "Aire", isNone: "Ninguno", tryIt: "Probar", done: "Hecho",
    about: "Acerca de", aboutVersion: "Kingbble 1.0, software libre bajo la GNU GPL v3.",
    aboutCredits: "Lista de palabras: ENABLE (dominio público). Leaves entrenadas con wolges (MIT, Andy Kurnia). App de escritorio: Electron (MIT). Fuentes: IBM Plex Sans y Young Serif (SIL Open Font License).",
    aboutTrademark: "Kingbble no está afiliado ni respaldado por Hasbro ni Mattel. Scrabble® es marca de sus titulares.",
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
const BONUS_LABEL = { en: { T: "×3", D: "×2", t: "3L", d: "2L" }, es: { T: "×3", D: "×2", t: "3L", d: "2L" } };

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
  sound: true, clock: "0", intro: true, introSound: "calm",
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
function noise(dur, { gain = 0.05, delay = 0, type = "bandpass", f0 = 1200, f1 = f0, q = 1 } = {}) {
  if (!settings.sound) return;
  try {
    audio = audio || new (window.AudioContext || window.webkitAudioContext)();
    const len = Math.ceil(audio.sampleRate * dur);
    const buf = audio.createBuffer(1, len, audio.sampleRate);
    const d = buf.getChannelData(0);
    for (let i = 0; i < len; i++) d[i] = Math.random() * 2 - 1;
    const src = audio.createBufferSource(), flt = audio.createBiquadFilter(), g = audio.createGain();
    const t0 = audio.currentTime + delay;
    src.buffer = buf;
    flt.type = type;
    flt.Q.value = q;
    flt.frequency.setValueAtTime(f0, t0);
    flt.frequency.exponentialRampToValueAtTime(f1, t0 + dur);
    g.gain.setValueAtTime(0.0001, t0);
    g.gain.exponentialRampToValueAtTime(gain, t0 + Math.min(0.02, dur / 3));
    g.gain.exponentialRampToValueAtTime(0.0001, t0 + dur);
    src.connect(flt).connect(g).connect(audio.destination);
    src.start(t0);
    src.stop(t0 + dur);
  } catch {}
}
// The intro, in step with the CSS animation: the tile lands (a wooden knock),
// the cape unfurls (cloth swish), the crown drops on (metal clink) and the
// glint passes (a soft shimmer).
// A soft voice: slow attack, long release, so nothing clicks or knocks.
function pad(freq, dur, gain, delay, { type = "sine", attack = 0.08, detune = 0 } = {}) {
  if (!settings.sound) return;
  try {
    audio = audio || new (window.AudioContext || window.webkitAudioContext)();
    const o = audio.createOscillator(), g = audio.createGain();
    const t0 = audio.currentTime + delay;
    o.type = type;
    o.frequency.value = freq;
    o.detune.value = detune;
    g.gain.setValueAtTime(0.0001, t0);
    g.gain.exponentialRampToValueAtTime(gain, t0 + attack);
    g.gain.exponentialRampToValueAtTime(0.0001, t0 + dur);
    o.connect(g).connect(audio.destination);
    o.start(t0);
    o.stop(t0 + dur + 0.05);
  } catch {}
}
// Intro sounds, each in step with the animation: the tile lands at ~0.45 s,
// the cape unfurls from 0.5 s, the crown settles at ~1.35 s, the glint at 1.6 s.
const INTRO_SOUNDS = {
  // Calm swell: a low warm note under the tile, a chord that opens with the
  // cape, and one quiet bell for the crown.
  calm: () => {
    pad(98, 2.8, 0.05, 0.42, { attack: 0.25 });
    [196, 247, 294].forEach((f, i) => pad(f, 2.4, 0.022, 0.55 + i * 0.12, { attack: 0.5, detune: i ? 4 : -4 }));
    pad(784, 1.8, 0.016, 1.45, { attack: 0.02 });
    pad(1175, 1.4, 0.008, 1.6, { attack: 0.3 });
  },
  // Music box: soft plucks rising with the logo, slow and sparse.
  musicbox: () => {
    [[523, 0.4], [659, 0.62], [784, 0.95], [1047, 1.45], [1319, 1.75]]
      .forEach(([f, d]) => { pad(f, 1.3, 0.03, d, { attack: 0.01 }); pad(f * 2, 0.6, 0.006, d, { attack: 0.01 }); });
  },
  // Breath: air rising with the cape and a single low hum; almost nothing.
  breath: () => {
    noise(1.1, { gain: 0.025, delay: 0.45, f0: 300, f1: 1400, q: 0.5 });
    pad(131, 2.6, 0.04, 0.42, { attack: 0.6 });
    pad(196, 2.2, 0.018, 1.45, { attack: 0.05 });
  },
};

// Resolves when the audio context runs (or after 400 ms, whatever happens).
function audioReady() {
  if (!settings.sound) return Promise.resolve();
  try {
    audio = audio || new (window.AudioContext || window.webkitAudioContext)();
    if (audio.state === "running") return Promise.resolve();
    return Promise.race([audio.resume(), new Promise((r) => setTimeout(r, 400))]).catch(() => {});
  } catch {
    return Promise.resolve();
  }
}

function introSound(force = false) {
  const kind = settings.introSound;
  if (!settings.sound || !INTRO_SOUNDS[kind]) return;
  if (!force && matchMedia("(prefers-reduced-motion: reduce)").matches) return;
  try {
    audio = audio || new (window.AudioContext || window.webkitAudioContext)();
    if (audio.state === "suspended") audio.resume();
  } catch {}
  INTRO_SOUNDS[kind]();
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
const reviewing = () => S && S.viewing < S.moves;
const myTurn = () => S && !S.over && isHuman(S.to_move) && !reviewing();
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
  while (S && !S.over && !isHuman(S.to_move) && !reviewing() && token === gameToken) {
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
  if (st.over && prev && !prev.over && !reviewing()) showOver();
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
  announceCursor();
}

// What a screen reader says about the cursor square, when the board has the focus.
let spoken = "";
function announceCursor() {
  const out = $("board-cursor");
  if (!cursor || document.activeElement !== $("board")) {
    spoken = "";
    return;
  }
  const { r, c, dir } = cursor;
  const k = key(r, c), ch = boardAt(r, c), p = pending.get(k);
  const bonus = S ? S.bonus[r][c] : ".";
  const what = ch !== "." ? ch.toUpperCase()
    : p ? t("cellPending", p.letter.toUpperCase())
    : [r === 7 && c === 7 ? t("cellCenter") : "", { T: t("cellTW"), D: t("cellDW"), t: t("cellTL"), d: t("cellDL") }[bonus] || "", t("cellEmpty")].filter(Boolean).join(", ");
  const text = `${COLS[c]}${r + 1}, ${what}, ${t(dir === "h" ? "dirH" : "dirV")}`;
  if (text !== spoken) out.textContent = spoken = text;
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
    te.tabIndex = 0;
    te.setAttribute("role", "button");
    const state = used.has(tile.id) ? t("tileUsed") : exchange && exchange.has(tile.id) ? t("tilePicked") : "";
    // The name is the letter shown plus hidden words, so it starts with what is seen.
    const v = (S && S.values[tile.ch]) || 0;
    const words = [tile.ch === "?" ? t("blankTile") : v === 1 ? t("tilePtsOne") : t("tilePts", v), state].filter(Boolean);
    te.querySelector(".v").setAttribute("aria-hidden", "true");
    te.insertAdjacentHTML("beforeend", `<span class="sr-only">${tile.ch === "?" ? "" : ", "}${words.join(", ")}</span>`);
    te.setAttribute("aria-pressed", String(selected === tile.id || !!(exchange && exchange.has(tile.id))));
    el.appendChild(te);
  }
  for (let i = rack.length; i < 7; i++) el.appendChild(Object.assign(document.createElement("div"), { className: "slot" }));
}

// The player whose rack is shown sits below the board, the other above it.
function placeBars() {
  const bottom = Math.max(0, shownSeat());
  $("bar-bottom").appendChild($(`p${bottom}`));
  $("bar-top").appendChild($(`p${1 - bottom}`));
}

let tab = "moves";
function showTabs() {
  const show = {
    moves: true,
    analysis: settings.hints && mode() !== "watch",
    tiles: settings.unseen || mode() === "watch",
    scoring: true,
  };
  if (!show[tab]) tab = "moves";
  for (const b of document.querySelectorAll(".tab")) {
    b.hidden = !show[b.dataset.tab];
    b.classList.toggle("on", b.dataset.tab === tab);
    b.setAttribute("aria-selected", String(b.dataset.tab === tab));
    b.tabIndex = b.dataset.tab === tab ? 0 : -1;
  }
  for (const pane of document.querySelectorAll(".tabpane")) pane.hidden = pane.dataset.pane !== tab;
}

function renderPanel() {
  if (!S) return;
  placeBars();
  for (const p of [0, 1]) {
    const el = $(`p${p}`);
    el.querySelector(".avatar").innerHTML = isHuman(p) ? name(p).charAt(0) : '<img src="logo.svg" alt="">';
    el.querySelector(".name").textContent = name(p);
    el.querySelector(".sub").textContent = isHuman(p) ? "" : (t("bots")[S.players[p]] || S.players[p]);
    el.querySelector(".score").textContent = S.scores[p];
    el.classList.toggle("turn", !S.over && S.to_move === p);
  }
  renderClocks();
  $("rv-pos").textContent = t("movePos", S.viewing, S.moves);
  document.body.classList.toggle("reviewing", reviewing());
  if (reviewing()) setStatus(t("reviewingStatus", S.viewing, S.moves));
  else if (S.over) setStatus(t("over"));
  else if (mode() === "watch" && paused) setStatus(t("paused"));
  else if (isHuman(S.to_move)) setStatus(humans().length === 1 ? t("yourTurn") : t("turnOf", name(S.to_move)));

  showTabs();
  renderScoring();
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

  $("card-racks").hidden = mode() !== "watch";
  if (mode() === "watch")
    $("racks").innerHTML = [0, 1].map((p) => `<div><span>${name(p)}</span><span class="mv">${S.racks[p] || ""}</span></div>`).join("");

  const h = $("history");
  h.innerHTML = "";
  S.history.forEach((e, i) => {
    const li = document.createElement("li");
    li.className = `p${e.player}${reviewing() && i === S.viewing - 1 ? " at" : ""}${i >= S.viewing ? " after" : ""}`;
    li.onclick = () => view(i + 1);
    const m = e.move;
    const text = m.kind === "place" ? m.text.replace(/ \(\d+\)$/, "") : m.kind === "exchange" ? t("exchanges", m.count) : t("passes");
    const score = m.kind === "place" ? `<span class="pts">+${m.score}</span>` : "";
    const rk = e.rack ? `<span class="rk">${e.rack}</span>` : "";
    const focus = reviewing() ? i === S.viewing - 1 : i === S.history.length - 1;
    const bd = focus && e.breakdown ? `<span class="bd">${breakdownText(e.breakdown)}</span>` : "";
    li.innerHTML = `<span class="n">${i + 1}</span><span><span class="who">${name(e.player)}</span><span class="mv">${text}</span>${score}${rk}${bd}</span><span class="cum">${e.cumulative}</span>`;
    h.appendChild(li);
  });
  if (S.over) {
    const li = document.createElement("li");
    li.className = "end";
    li.innerHTML = `<span class="n"></span><span>${t("over")}</span><span class="cum">${S.scores[0]}–${S.scores[1]}</span>`;
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
  const rv = reviewing();
  $("controls").hidden = watch || rv;
  $("watch-controls").hidden = !watch || rv;
  $("review-bar").hidden = !rv;
  $("rv-first").disabled = $("rv-prev").disabled = !S || S.viewing === 0 || busy;
  $("rv-next").disabled = $("rv-last").disabled = !rv || busy;
  if (rv) {
    $("rv-text").textContent = t("reviewingText", S.viewing, S.moves);
    $("rv-rewind").disabled = busy;
  }
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

// ---------- scoring ----------

// "NOTARIZE 18×3 = 54 · bingo +50 · 104 pts", "ORGY 16 + ORE 3 · 19 pts"
function breakdownText(b) {
  if (!b) return "";
  const parts = [b.words.map((w) => (w.mult > 1 ? `${w.word} ${w.sum}×${w.mult} = ${w.score}` : `${w.word} ${w.score}`)).join(" + ")];
  if (b.bingo) parts.push(`${t("bingo")} +${b.bingo}`);
  parts.push(t("points2", b.total));
  return parts.join(" · ");
}

const COUNTS = { "?": 2, A: 9, B: 2, C: 2, D: 4, E: 12, F: 2, G: 3, H: 2, I: 9, J: 1, K: 1, L: 4, M: 2, N: 6, O: 8, P: 2, Q: 1, R: 6, S: 4, T: 6, U: 4, V: 2, W: 2, X: 1, Y: 2, Z: 1 };

function renderScoring() {
  const labels = BONUS_LABEL[settings.lang] || BONUS_LABEL.en;
  $("legend").innerHTML = [["T", "tw", "sqTW"], ["D", "dw", "sqDW"], ["t", "tl", "sqTL"], ["d", "dl", "sqDL"]]
    .map(([k, cls, txt]) => `<li><span class="sq cell ${cls}">${labels[k]}</span><span>${t(txt)}</span></li>`)
    .join("") + `<li><span class="sq cell center"></span><span>${t("sqCenter")}</span></li>`;
  const ex = t("rulesEx");
  $("rules").innerHTML = t("rules").map((r, i) => `<li>${r}${ex[i] ? `<span class="ex">${ex[i]}</span>` : ""}</li>`).join("");
  const values = S ? S.values : null;
  $("values").innerHTML = Object.entries(COUNTS)
    .map(([l, n]) => `<span><b>${l === "?" ? "?" : l}</b><em>${values ? values[l] : ""}</em><small>×${n}</small></span>`)
    .join("");
}

// ---------- preview ----------

function placementArgs() {
  return [...pending.values()].map((p) => `${p.r},${p.c},${p.blank ? p.letter.toLowerCase() : p.letter}`);
}

function schedulePreview() {
  clearTimeout(checkTimer);
  const pv = $("preview");
  const gain = (n) => document.querySelectorAll(".player .gain").forEach((el) => { el.textContent = n != null && el.closest(".player").id === `p${S && S.to_move}` ? `+${n}` : ""; });
  if (!pending.size || !myTurn() || !settings.preview) {
    gain(null);
    pv.textContent = "";
    pv.className = "preview";
    return;
  }
  const args = placementArgs();
  checkTimer = setTimeout(async () => {
    const r = await api("check", args).catch(() => null);
    if (!r || placementArgs().join() !== args.join()) return;
    pv.className = `preview ${r.ok ? "ok" : "bad"}`;
    gain(r.ok ? r.score : null);
    pv.textContent = r.ok ? `${r.move.text.replace(/ \(\d+\)$/, "").split(" ")[0]} · ${breakdownText(r.breakdown) || t("points", r.score)}` : tr(r.error);
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
    // The size of a board square, the size it will have once dropped.
    const w = Math.round($("board").firstElementChild.getBoundingClientRect().width) || 40;
    Object.assign(drag.ghost.style, { width: `${w}px`, height: `${w}px`, fontSize: `${Math.round(w * 0.6)}px` });
    drag.ghost.querySelector(".v")?.style.setProperty("font-size", `${Math.max(8, Math.round(w * 0.24))}px`);
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
  if (cell && (isFree(+cell.dataset.r, +cell.dataset.c) || pending.has(key(+cell.dataset.r, +cell.dataset.c)))) return cell;
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
    const r = +target.dataset.r, c = +target.dataset.c, tk = key(r, c);
    if (tk === d.pkey) return render();
    // A tile not yet played is covered: from the rack it goes back to the rack,
    // from the board the two swap squares (blanks keep their letter).
    const other = pending.get(tk);
    if (other) unplace(tk);
    if (p) unplace(d.pkey);
    const ok = await place(id, r, c, p && p.blank ? p.letter : null);
    if (ok && other && p) {
      const [sr, sc] = d.pkey.split(",").map(Number);
      await place(other.id, sr, sc, other.blank ? other.letter : null);
    }
    if (!ok) render();
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
  // Keys meant for a focused button, tab or rack tile stay theirs.
  if (e.target.closest("button, a, [role=tab], #rack") && ["Enter", " ", "ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown"].includes(e.key)) return;
  const onBoard = e.target === $("board");
  if (onBoard && e.key === " " && cursor && !reviewing() && mode() !== "watch") {
    cursor = { ...cursor, dir: cursor.dir === "h" ? "v" : "h" };
    renderBoard();
    e.preventDefault();
    return;
  }
  if (S && (e.key === "ArrowLeft" || e.key === "ArrowRight") && (!cursor || reviewing())) {
    view(S.viewing + (e.key === "ArrowLeft" ? -1 : 1));
    e.preventDefault();
    return;
  }
  if (reviewing()) {
    if (e.key === "Escape") view(S.moves);
    return;
  }
  if (mode() === "watch") {
    if (e.key === " ") {
      togglePause();
      e.preventDefault();
    }
    return;
  }
  if (e.key === "Enter") return void play();
  if (e.key === "Escape") return void recall();
  if (!canType() && !onBoard) return;
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
    // From the keyboard the arrows only move; Space changes the direction.
    const dir = onBoard ? cursor.dir : dc ? "h" : "v";
    cursor = { r: Math.min(N - 1, Math.max(0, cursor.r + dr)), c: Math.min(N - 1, Math.max(0, cursor.c + dc)), dir };
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
  r.hints.forEach((h, i) => {
    const li = document.createElement("li");
    const m = h.move;
    const text = m.kind === "place" ? m.text : m.kind === "exchange" ? `${t("exchange")} ${m.exchanged}` : t("pass");
    li.innerHTML = `<span class="n">${i + 1}</span><span class="mv">${text}</span><span class="eq">${h.equity.toFixed(1)}</span>`;
    li.onclick = () => loadHint(m);
    ol.appendChild(li);
  });
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

// Shows the position after the first `n` moves; n >= moves goes back to the game.
async function view(n) {
  if (!S || busy) return;
  n = Math.max(0, Math.min(n, S.moves));
  pending.clear();
  order = [];
  exchange = null;
  cursor = null;
  if (n === S.viewing) return;
  if (mode() === "watch" && !paused && n < S.moves) togglePause();
  await run("view", [n]);
  if (!reviewing()) botLoop();
}

async function rewind() {
  if (!reviewing()) return;
  gameToken++;
  const r = await run("rewind", [S.viewing]);
  if (r) botLoop();
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
    div.innerHTML = `<span class="title" id="bot-title-${i}">${title}</span><select data-bot="${i}" aria-labelledby="bot-title-${i}">${opts}</select>
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
  for (const b of $("mode").querySelectorAll("button")) {
    b.classList.toggle("on", b.dataset.mode === dlgMode);
    b.setAttribute("aria-checked", String(b.dataset.mode === dlgMode));
  }
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
  for (const el of document.querySelectorAll("[data-i18n-aria]")) el.setAttribute("aria-label", t(el.dataset.i18nAria));
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
  $("try-intro").onclick = () => introSound(true);
  d.showModal();
  refreshLeaves();
}

// ---------- leaves training (local only; the server hides it elsewhere) ----------

let leavesTimer = null;
const leavesPost = (path, body) =>
  fetch("/api/leaves" + path, { method: "POST", body: JSON.stringify(body || {}) }).then((r) => r.json());

function fmtEta(s) {
  if (s == null) return "";
  const m = Math.round(s / 60);
  return m < 90 ? `${Math.max(m, 1)} min` : `${(m / 60).toFixed(1)} h`;
}

function renderLeaves(info) {
  const sec = $("leaves-section");
  sec.hidden = !info || !info.ok || !info.enabled;
  if (sec.hidden) return false;
  const sel = $("lv-lex");
  const job = info.job || { state: "idle" };
  const want = sel.value || job.lexicon || settings.lexicon || info.lexica[0]?.name;
  if (sel.options.length !== info.lexica.length) {
    sel.innerHTML = "";
    for (const l of info.lexica) sel.add(new Option(l.name, l.name));
  }
  sel.value = want;
  const cur = info.lexica.find((l) => l.name === sel.value);
  const names = { own: t("lvOwnNow"), default: t("lvDefaultNow"), none: t("lvNone") };
  $("lv-now").textContent = cur ? names[cur.leaves] : "";
  $("lv-use-row").hidden = !cur || !cur.has_own;
  if (cur && cur.has_own) $("lv-use").value = cur.leaves === "own" ? "own" : "default";
  const running = job.state === "running";
  $("lv-train").hidden = running;
  $("lv-cancel").hidden = !running;
  $("lv-lex").disabled = $("lv-level").disabled = running;
  $("lv-bar").hidden = !running;
  $("lv-bar").firstElementChild.style.width = `${job.pct || 0}%`;
  let msg = "";
  if (running) {
    msg = t("lvRunning", job.lexicon) + (job.eta_s != null ? ` ${t("lvEta", fmtEta(job.eta_s))}` : "");
  } else if (job.state === "done" && job.result) {
    const r = job.result;
    msg = t("lvResult", r.win_pct.toFixed(1), r.games, r.ci_lo.toFixed(1), r.ci_hi.toFixed(1),
      t({ better: "lvBetter", equal: "lvEqual", worse: "lvWorse" }[r.verdict]));
  } else if (job.state === "done") msg = t("lvDone");
  else if (job.state === "failed") msg = `${t("lvFailed")}${job.message ? ": " + job.message : ""}`;
  else if (job.state === "cancelled") msg = t("lvStopped");
  $("lv-msg").textContent = msg;
  return running;
}

async function refreshLeaves() {
  clearTimeout(leavesTimer);
  let info = null;
  try {
    info = await fetch("/api/leaves", { cache: "no-store" }).then((r) => r.json());
  } catch {}
  const running = renderLeaves(info);
  if ($("dlg-settings").open && info && info.ok && info.enabled) {
    leavesTimer = setTimeout(refreshLeaves, running ? 2000 : 10000);
  }
}

function initLeaves() {
  $("lv-lex").onchange = refreshLeaves;
  $("lv-train").onclick = async () => {
    const r = await leavesPost("/train", { lexicon: $("lv-lex").value, level: $("lv-level").value });
    if (!r.ok) toast(r.error || "error");
    refreshLeaves();
  };
  $("lv-cancel").onclick = async () => {
    await leavesPost("/cancel");
    refreshLeaves();
  };
  $("lv-use").onchange = async () => {
    const r = await leavesPost("/use", { lexicon: $("lv-lex").value, which: $("lv-use").value });
    if (!r.ok) toast(r.error || "error");
    refreshLeaves();
  };
  $("dlg-settings").addEventListener("close", () => clearTimeout(leavesTimer));
}

// ---------- start ----------

// Things waiting for the intro to end, such as the first New game dialog.
let splashing = true;
const afterSplash = [];
function whenSplashDone(fn) {
  splashing ? afterSplash.push(fn) : fn();
}
function splashEnded() {
  splashing = false;
  afterSplash.splice(0).forEach((fn) => fn());
}

function splash() {
  const el = $("splash");
  if (!settings.intro || matchMedia("(prefers-reduced-motion: reduce)").matches) {
    el.remove();
    return splashEnded();
  }
  const end = () => {
    el.classList.add("done");
    setTimeout(() => el.remove(), 600);
    splashEnded();
  };
  const start = async () => {
    // Start the animation only once the sound can really play, so both share t = 0.
    await audioReady();
    el.classList.remove("wait");
    introSound();
    const timer = setTimeout(end, 3000);
    el.addEventListener("click", () => {
      clearTimeout(timer);
      end();
    }, { once: true });
  };
  // A browser only lets a page play sound after a click: on the web, with a sound
  // chosen, the intro waits for one so the sound starts with the animation.
  // The desktop app may play it at once.
  const sound = settings.sound && INTRO_SOUNDS[settings.introSound];
  if (!sound || navigator.userAgent.includes("Electron")) return start();
  el.classList.add("wait");
  el.removeAttribute("aria-hidden");
  const gate = splashGate();
  el.appendChild(gate);
  const btn = gate.querySelector(".splash-enter");
  btn.focus({ focusVisible: false }); // Enter works at once; no ring until the keyboard is used
  btn.addEventListener("click", (e) => {
    e.stopPropagation();
    btn.disabled = true;
    gate.classList.add("out");
    el.setAttribute("aria-hidden", "true");
    audioReady(); // unlock now, inside the click
    setTimeout(() => {
      gate.remove();
      start();
    }, 380);
  }, { once: true });
}

// The waiting screen: the name in tiles, the Start button, and tiles drifting
// slowly behind them, a few near and sharp, more far, small and faint.
const TILE_POINTS = { A: 1, B: 3, C: 3, D: 2, E: 1, F: 4, G: 2, H: 4, I: 1, J: 8, K: 5, L: 1, M: 3, N: 1, O: 1, P: 3, Q: 10, R: 1, S: 1, T: 1, U: 1, V: 4, W: 4, X: 8, Y: 4, Z: 10 };
function splashGate() {
  const gate = document.createElement("div");
  gate.className = "splash-gate";
  const tile = (ch, extra = "") => `<span class="gtile${extra}">${ch}<i>${TILE_POINTS[ch] || ""}</i></span>`;
  const bag = "EEEEAAAIIIOOONNRRTTLSSUDGBCMPFHVWYKJXQZ";
  const rnd = (a, b) => a + Math.random() * (b - a);
  let drift = "";
  for (let i = 0; i < 22; i++) {
    const near = i < 5;
    const size = near ? rnd(46, 64) : rnd(22, 40);
    const style = [
      `left:${rnd(-2, 96).toFixed(1)}%`, `top:${rnd(-2, 94).toFixed(1)}%`, `--s:${size.toFixed(0)}px`,
      `--o:${(near ? rnd(0.16, 0.24) : rnd(0.06, 0.13)).toFixed(2)}`, `--b:${near ? 0 : rnd(0, 1.6).toFixed(1)}px`,
      `--r0:${rnd(-18, 18).toFixed(0)}deg`, `--r1:${rnd(-18, 18).toFixed(0)}deg`,
      `--dx:${rnd(-9, 9).toFixed(1)}vw`, `--dy:${rnd(-9, 9).toFixed(1)}vh`,
      `animation-duration:${rnd(38, 80).toFixed(0)}s`, `animation-delay:-${rnd(0, 60).toFixed(0)}s`,
    ].join(";");
    drift += tile(bag[Math.floor(Math.random() * bag.length)]).replace('class="gtile"', `class="gtile drift" style="${style}"`);
  }
  gate.innerHTML = `<div class="gate-drift" aria-hidden="true">${drift}</div>
    <div class="gate-center">
      <div class="gate-word" role="img" aria-label="Kingbble">${[..."KINGBBLE"].map((c) => tile(c)).join("")}</div>
      <button class="splash-enter"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M5 3.2v9.6L12.6 8z"/></svg><span>${t("enter")}</span></button>
    </div>`;
  return gate;
}

async function init() {
  splash();
  initLeaves();
  for (const p of [0, 1]) {
    const el = document.createElement("div");
    el.className = "player";
    el.id = `p${p}`;
    el.innerHTML = `<div class="avatar"></div><div class="pinfo"><span class="name"></span><span class="sub"></span></div><span class="turn-dot"></span><div class="pnums"><span class="clock"></span><span class="gain"></span><span class="score">0</span></div>`;
    $(p ? "bar-top" : "bar-bottom").appendChild(el);
  }
  for (const b of document.querySelectorAll(".tab"))
    b.onclick = () => {
      tab = b.dataset.tab;
      showTabs();
      if (tab === "analysis" && myTurn() && !$("hints").children.length) hint();
    };
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
  // Keyboard play: focusing the board puts the cursor on the centre, or where play left off.
  $("board").addEventListener("focus", () => {
    // Only for keyboard focus: a mouse click on a square already sets the cursor,
    // and redrawing here would swallow that click.
    if (!$("board").matches(":focus-visible")) return;
    if (!cursor && S && myTurn() && !reviewing()) {
      const k = order[order.length - 1];
      const p = k && pending.get(k);
      cursor = p ? { ...(advance(p.r, p.c, "h") || p), dir: "h" } : { r: 7, c: 7, dir: "h" };
    }
    renderBoard();
  });
  $("board").addEventListener("blur", () => {
    spoken = "";
    $("board-cursor").textContent = "";
  });
  $("rack").addEventListener("keydown", (e) => {
    const tile = e.target.closest(".tile[data-id]");
    if (!tile) return;
    const tiles = [...$("rack").querySelectorAll(".tile[data-id]")];
    const i = tiles.indexOf(tile);
    if (e.key === "Enter" || e.key === " ") {
      e.preventDefault();
      if (tile.classList.contains("used")) return;
      const id = +tile.dataset.id;
      tapTile({ id });
      $("rack").querySelector(`.tile[data-id="${id}"]`)?.focus();
    } else if (e.key === "ArrowLeft" || e.key === "ArrowRight") {
      e.preventDefault();
      tiles[(i + (e.key === "ArrowLeft" ? -1 : 1) + tiles.length) % tiles.length].focus();
    }
  });
  $("board").closest("main").querySelector(".tabs").addEventListener("keydown", (e) => {
    if (e.key !== "ArrowLeft" && e.key !== "ArrowRight" && e.key !== "Home" && e.key !== "End") return;
    const tabs = [...document.querySelectorAll(".tab")].filter((b) => !b.hidden);
    let i = tabs.indexOf(document.activeElement);
    i = e.key === "Home" ? 0 : e.key === "End" ? tabs.length - 1 : (i + (e.key === "ArrowLeft" ? -1 : 1) + tabs.length) % tabs.length;
    tabs[i].click();
    tabs[i].focus();
    e.preventDefault();
  });
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
  $("rv-first").onclick = () => view(0);
  $("rv-prev").onclick = () => view(S.viewing - 1);
  $("rv-next").onclick = () => view(S.viewing + 1);
  $("rv-last").onclick = () => view(S.moves);
  $("rv-live").onclick = () => view(S.moves);
  $("rv-rewind").onclick = rewind;
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
  } else whenSplashDone(newGame);
}

init();
