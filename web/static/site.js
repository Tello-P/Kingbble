// The website's few behaviours: theme (shared with the game's settings), the
// chapter menu on small screens, the current section in "On this page", and the
// tiles drifting behind the home page's title.
"use strict";
(() => {
  const KEY = "kingbble-settings";
  const read = () => {
    try { return JSON.parse(localStorage.getItem(KEY) || "{}"); } catch { return {}; }
  };
  const write = (patch) => {
    try { localStorage.setItem(KEY, JSON.stringify({ ...read(), ...patch })); } catch {}
  };
  const root = document.documentElement;
  // Remember the language of the page being read (the demo and the entry page follow it).
  write({ lang: root.lang === "es" ? "es" : "en" });

  const themeBtn = document.getElementById("theme");
  if (themeBtn) {
    themeBtn.addEventListener("click", () => {
      const dark = root.dataset.theme
        ? root.dataset.theme === "dark"
        : matchMedia("(prefers-color-scheme: dark)").matches;
      root.dataset.theme = dark ? "light" : "dark";
      write({ theme: root.dataset.theme });
    });
  }

  const menu = document.getElementById("menu");
  const index = document.getElementById("index");
  if (menu && index) {
    const set = (open) => {
      index.classList.toggle("open", open);
      menu.setAttribute("aria-expanded", String(open));
      document.body.style.overflow = open ? "hidden" : "";
    };
    menu.addEventListener("click", () => set(!index.classList.contains("open")));
    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape" && index.classList.contains("open")) {
        set(false);
        menu.focus();
      }
    });
    const cur = index.querySelector("[aria-current]");
    if (cur && index.scrollHeight > index.clientHeight) cur.scrollIntoView({ block: "center" });
  }

  const links = [...document.querySelectorAll(".toc a")];
  if (links.length && "IntersectionObserver" in window) {
    const byId = new Map(links.map((a) => [a.getAttribute("href").slice(1), a]));
    const seen = new Set();
    const io = new IntersectionObserver((entries) => {
      for (const e of entries) e.isIntersecting ? seen.add(e.target.id) : seen.delete(e.target.id);
      const first = [...byId.keys()].find((id) => seen.has(id));
      if (first) for (const [id, a] of byId) a.classList.toggle("on", id === first);
    }, { rootMargin: "-70px 0px -60% 0px" });
    for (const id of byId.keys()) {
      const h = document.getElementById(id);
      if (h) io.observe(h);
    }
  }

  const drift = document.querySelector(".hero-drift");
  if (drift && !matchMedia("(prefers-reduced-motion: reduce)").matches) {
    const POINTS = { A: 1, B: 3, C: 3, D: 2, E: 1, F: 4, G: 2, H: 4, I: 1, J: 8, K: 5, L: 1, M: 3, N: 1, O: 1, P: 3, Q: 10, R: 1, S: 1, T: 1, U: 1, V: 4, W: 4, X: 8, Y: 4, Z: 10 };
    const bag = "EEEEAAAIIIOOONNRRTTLSSUDGBCMPFHVWYKJXQZ";
    const rnd = (a, b) => a + Math.random() * (b - a);
    let out = "";
    for (let i = 0; i < 18; i++) {
      const near = i < 4;
      const ch = bag[Math.floor(Math.random() * bag.length)];
      const style = [
        `left:${rnd(46, 98).toFixed(1)}%`, `top:${rnd(-4, 92).toFixed(1)}%`, `--s:${rnd(near ? 46 : 22, near ? 66 : 40).toFixed(0)}px`,
        `--o:${(near ? rnd(0.14, 0.22) : rnd(0.05, 0.12)).toFixed(2)}`, `--b:${near ? 0 : rnd(0, 1.6).toFixed(1)}px`,
        `--r0:${rnd(-18, 18).toFixed(0)}deg`, `--r1:${rnd(-18, 18).toFixed(0)}deg`,
        `--dx:${rnd(-8, 8).toFixed(1)}vw`, `--dy:${rnd(-8, 8).toFixed(1)}vh`,
        `animation-duration:${rnd(38, 80).toFixed(0)}s`, `animation-delay:-${rnd(0, 60).toFixed(0)}s`,
      ].join(";");
      out += `<span class="wtile" style="${style}">${ch}<i>${POINTS[ch]}</i></span>`;
    }
    drift.innerHTML = out;
  }
})();
