#!/usr/bin/env python3
"""Builds the Kingbble website into web/dist: a static site (GitHub Pages) in
English and Spanish, with the playable demo (the interface of ui/static over
the engine compiled to WebAssembly).

    python web/build.py [--out web/dist]

Pages are web/content/SLUG.LANG.md: a few "key: value" lines, a line "---" and
the text, in a small Markdown (headings, paragraphs, lists, tables, code,
links to other pages by slug) plus ":::" blocks (note, stats, bars, flow,
board, shot, raw). Only the standard library and Pillow (for the screenshots).
Every link is relative, so the site works under any base path.
"""

from __future__ import annotations

import argparse
import html
import re
import shutil
from pathlib import Path

WEB = Path(__file__).resolve().parent
ROOT = WEB.parent
LANGS = ("en", "es")

# The chapters, in order, by part. "home" pages are laid out by their own template.
PARTS = [
    ("start", {"en": "Kingbble", "es": "Kingbble"}, ["index", "idea", "tour"]),
    ("engine", {"en": "The engine", "es": "El motor"},
     ["lexicon", "movegen", "scoring", "equity", "simulation", "endgame", "preendgame"]),
    ("rival", {"en": "Reading the opponent", "es": "Leer al rival"},
     ["data", "human-model", "inference", "risk"]),
    ("measure", {"en": "Measuring", "es": "Medir"},
     ["method", "results", "references", "lessons"]),
    ("use", {"en": "Using it", "es": "Usarlo"},
     ["play", "levels", "interface", "download", "training", "build"]),
    ("more", {"en": "Appendix", "es": "Apéndice"},
     ["roadmap", "glossary", "faq", "about"]),
]
ORDER = [slug for _, _, slugs in PARTS for slug in slugs]

UI = {
    "en": {
        "manual": "Manual", "demo": "Play the demo", "demo_short": "Demo", "download": "Download",
        "contents": "Contents", "on_page": "On this page", "prev": "Previous", "next": "Next",
        "chapter": "Chapter", "theme": "Light or dark", "menu": "Chapters", "skip": "Skip to the text",
        "other": "Español", "other_title": "Leer en español",
        "foot": "Kingbble is free software under the GPL-3.0. It is not affiliated with or endorsed by Hasbro, Mattel or "
                "any owner of a crossword board game trademark; such names are used only to describe compatibility.",
        "colophon": "Set in Young Serif and IBM Plex Sans. Every figure on this site comes from a run of the project, "
                    "with its number of games and its 95 % interval.",
        "source": "Source code", "home": "Home", "404": "This square is empty",
        "404_text": "There is no page at this address. The contents are a good place to start again.",
        "tagline": "A crossword-game engine that tries to guess your rack",
    },
    "es": {
        "manual": "Manual", "demo": "Jugar la demo", "demo_short": "Demo", "download": "Descargar",
        "contents": "Contenido", "on_page": "En esta página", "prev": "Anterior", "next": "Siguiente",
        "chapter": "Capítulo", "theme": "Claro u oscuro", "menu": "Capítulos", "skip": "Saltar al texto",
        "other": "English", "other_title": "Read in English",
        "foot": "Kingbble es software libre bajo la GPL-3.0. No está afiliado ni respaldado por Hasbro, Mattel ni ningún "
                "titular de marcas de juegos de palabras cruzadas; esos nombres solo se usan para describir compatibilidad.",
        "colophon": "Compuesto en Young Serif e IBM Plex Sans. Todas las cifras de esta web salen de una ejecución del "
                    "proyecto, con su número de partidas y su intervalo del 95 %.",
        "source": "Código fuente", "home": "Inicio", "404": "Esta casilla está vacía",
        "404_text": "No hay ninguna página en esta dirección. El contenido es un buen sitio para volver a empezar.",
        "tagline": "Un motor de palabras cruzadas que intenta adivinar tu atril",
    },
}

REPO_URL = "https://github.com/Tello-P/Kingbble"
TILE_POINTS = dict(zip("ABCDEFGHIJKLMNOPQRSTUVWXYZ",
                       [1, 3, 3, 2, 1, 4, 2, 4, 1, 8, 5, 1, 3, 1, 1, 3, 10, 1, 1, 1, 1, 4, 4, 8, 4, 10]))
BONUS = [
    "T..d...T...d..T", ".D...t...t...D.", "..D...d.d...D..", "d..D...d...D..d", "....D.....D....",
    ".t...t...t...t.", "..d...d.d...d..", "T..d...*...d..T",
]
BONUS = BONUS + BONUS[-2::-1]


def esc(text: str) -> str:
    return html.escape(text, quote=True)


def slugify(text: str) -> str:
    text = re.sub(r"<[^>]+>", "", text).lower()
    table = str.maketrans("áéíóúüñ", "aeiouun")
    text = text.translate(table)
    return re.sub(r"[^a-z0-9]+", "-", text).strip("-") or "s"


# ---------- the small Markdown ----------

class Page:
    def __init__(self, slug: str, lang: str, path: Path):
        self.slug, self.lang = slug, lang
        head, _, body = path.read_text(encoding="utf-8").partition("\n---\n")
        self.meta = {}
        for line in head.splitlines():
            if ":" in line:
                k, v = line.split(":", 1)
                self.meta[k.strip()] = v.strip()
        self.title = self.meta["title"]
        self.nav = self.meta.get("nav", self.title)
        self.lead = self.meta.get("lead", "")
        self.toc: list[tuple[str, str]] = []
        self.body = body

    def href(self, target: str) -> str:
        """A link target as written in the text: a slug, slug#anchor, 'demo', or a URL."""
        if re.match(r"^(https?:|mailto:|#|\.\./|/)", target):
            return target
        if target == "demo":
            return f"../demo/index.html?lang={self.lang}"
        if target == "repo":
            return REPO_URL
        if target.startswith("repo:"):
            return f"{REPO_URL}/{target[5:]}"
        slug, _, anchor = target.partition("#")
        if slug not in ORDER:
            raise SystemExit(f"{self.slug}.{self.lang}: unknown link target '{target}'")
        return f"{slug}.html" + (f"#{anchor}" if anchor else "")

    def inline(self, text: str) -> str:
        out, codes = "", []

        def keep(m):
            codes.append(f"<code>{esc(m.group(1))}</code>")
            return f"\x00{len(codes) - 1}\x00"

        text = re.sub(r"`([^`]+)`", keep, text)
        text = esc(text)
        text = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)",
                      lambda m: f'<a href="{esc(self.href(html.unescape(m.group(2))))}">{m.group(1)}</a>', text)
        text = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", text)
        text = re.sub(r"(?<![\w*])\*([^*\s][^*]*)\*(?!\w)", r"<em>\1</em>", text)
        out = re.sub(r"\x00(\d+)\x00", lambda m: codes[int(m.group(1))], text)
        return out

    # ----- blocks -----

    def block(self, kind: str, arg: str, lines: list[str]) -> str:
        rows = [[c.strip() for c in ln.split("|")] for ln in lines if ln.strip()]
        if kind == "note":
            title = f"<p class=\"note-title\">{self.inline(arg)}</p>" if arg else ""
            return f'<aside class="note">{title}{self.render(lines)}</aside>'
        if kind == "stats":
            items = "".join(
                f'<div class="stat"><dt>{self.inline(r[0])}</dt><dd>{self.inline(r[1])}</dd></div>' for r in rows)
            return f'<dl class="stats">{items}</dl>'
        if kind == "flow":
            items = "".join(
                f'<li><b>{self.inline(r[0])}</b><span>{self.inline(r[1]) if len(r) > 1 else ""}</span></li>'
                for r in rows)
            return f'<ol class="flow">{items}</ol>'
        if kind == "bars":
            return self.bars(arg, rows)
        if kind == "board":
            return self.board(arg, lines)
        if kind == "tiles":
            return f'<p class="tileword" role="img" aria-label="{esc(arg)}">{tiles(arg)}</p>'
        if kind == "shot":
            name, _, caption = arg.partition("|")
            return (f'<figure class="shot"><img src="../assets/img/{esc(name.strip())}" alt="{esc(caption.strip())}" '
                    f'loading="lazy"><figcaption>{self.inline(caption.strip())}</figcaption></figure>')
        if kind == "raw":
            return "\n".join(lines)
        raise SystemExit(f"{self.slug}.{self.lang}: unknown block '{kind}'")

    def bars(self, arg: str, rows: list[list[str]]) -> str:
        """Interval chart: 'label | value | lo | hi | note' rows; options min= max= ref= unit= caption=..."""
        caption = ""
        if "caption=" in arg:
            arg, caption = arg.split("caption=", 1)
        opt = dict(kv.split("=", 1) for kv in arg.split() if "=" in kv)
        lo, hi = float(opt.get("min", 0)), float(opt.get("max", 100))
        unit = opt.get("unit", "")
        ref = opt.get("ref")
        pct = lambda v: f"{(float(v) - lo) / (hi - lo) * 100:.2f}%"
        fmt = (lambda v: v.replace(".", ",")) if self.lang == "es" else (lambda v: v)
        out = []
        for r in rows:
            label, value = r[0], r[1]
            has_ci = len(r) > 3 and r[2] and r[3]
            note = r[4] if len(r) > 4 else ""
            mark = f'<i class="pt" style="left:{pct(value)}"></i>'
            if has_ci:
                mark = (f'<i class="ci" style="left:{pct(r[2])};width:calc({pct(r[3])} - {pct(r[2])})"></i>' + mark)
                rng = f' <small>({fmt(r[2])}–{fmt(r[3])})</small>'
            else:
                mark = f'<i class="fill" style="width:{pct(value)}"></i>'
                rng = ""
            refline = f'<i class="ref" style="left:{pct(ref)}"></i>' if ref else ""
            cls = " sig" if has_ci and ref and (float(r[2]) > float(ref) or float(r[3]) < float(ref)) else ""
            out.append(
                f'<div class="bar{cls}"><div class="bar-label">{self.inline(label)}'
                + (f'<small>{self.inline(note)}</small>' if note else "")
                + f'</div><div class="bar-track">{refline}{mark}</div>'
                  f'<div class="bar-value">{fmt(value)}{esc(unit)}{rng}</div></div>')
        cap = f"<figcaption>{self.inline(caption.strip())}</figcaption>" if caption else ""
        return f'<figure class="bars">{"".join(out)}{cap}</figure>'

    def board(self, arg: str, lines: list[str]) -> str:
        """A piece of board: rows of characters. Letters are tiles (lowercase: a blank),
        '.' an empty square with its real bonus, '+' an anchor, '#' a highlighted empty
        square. arg: 'ROW COL | caption' gives the board position of the top-left square."""
        pos, _, caption = arg.partition("|")
        r0, c0 = (int(x) for x in (pos.split() + ["0", "0"])[:2])
        rows = [ln.rstrip("\n") for ln in lines if ln.strip()]
        width = max(len(r) for r in rows)
        cells = []
        for i, row in enumerate(rows):
            for j in range(width):
                ch = row[j] if j < len(row) else "."
                bonus = BONUS[(r0 + i) % 15][(c0 + j) % 15]
                cls = {"T": " tw", "D": " dw", "*": " dw", "t": " tl", "d": " dl"}.get(bonus, "")
                if ch.isalpha():
                    up = ch.upper()
                    pts = "" if ch.islower() else TILE_POINTS[up]
                    blank = " blank" if ch.islower() else ""
                    cells.append(f'<span class="sq"><span class="mtile{blank}">{up}<i>{pts}</i></span></span>')
                elif ch == "+":
                    cells.append(f'<span class="sq{cls} anchor"></span>')
                elif ch == "#":
                    cells.append(f'<span class="sq{cls} hl"></span>')
                else:
                    cells.append(f'<span class="sq{cls}"></span>')
        cap = f"<figcaption>{self.inline(caption.strip())}</figcaption>" if caption.strip() else ""
        return (f'<figure class="miniboard"><div class="mb" style="--w:{width}" aria-hidden="true">{"".join(cells)}</div>'
                f'{cap}</figure>')

    def render(self, lines: list[str]) -> str:
        out, i, n = [], 0, len(lines)
        para: list[str] = []

        def flush():
            if para:
                out.append(f"<p>{self.inline(' '.join(para))}</p>")
                para.clear()

        while i < n:
            line = lines[i]
            if not line.strip():
                flush()
                i += 1
            elif line.startswith(":::"):
                flush()
                kind, _, arg = line[3:].strip().partition(" ")
                j = i + 1
                depth = 1
                body = []
                while j < n:
                    if lines[j].strip() == ":::":
                        depth -= 1
                        if depth == 0:
                            break
                    elif lines[j].startswith(":::"):
                        depth += 1
                    body.append(lines[j])
                    j += 1
                out.append(self.block(kind, arg.strip(), body))
                i = j + 1
            elif line.startswith("```"):
                flush()
                j = i + 1
                code = []
                while j < n and not lines[j].startswith("```"):
                    code.append(lines[j])
                    j += 1
                out.append(f'<pre tabindex="0"><code>{esc(chr(10).join(code))}</code></pre>')
                i = j + 1
            elif m := re.match(r"^(#{2,3}) (.+)$", line):
                flush()
                level = len(m.group(1))
                text = self.inline(m.group(2))
                anchor = slugify(text)
                if level == 2:
                    self.toc.append((anchor, re.sub(r"<[^>]+>", "", text)))
                out.append(f'<h{level} id="{anchor}">{text}</h{level}>')
                i += 1
            elif re.match(r"^(- |\d+\. )", line):
                flush()
                ordered = not line.startswith("- ")
                items = []
                while i < n and (re.match(r"^(- |\d+\. )", lines[i]) or (lines[i].startswith("  ") and lines[i].strip())):
                    if re.match(r"^(- |\d+\. )", lines[i]):
                        items.append(re.sub(r"^(- |\d+\. )", "", lines[i]))
                    else:
                        items[-1] += " " + lines[i].strip()
                    i += 1
                tag = "ol" if ordered else "ul"
                out.append(f"<{tag}>" + "".join(f"<li>{self.inline(x)}</li>" for x in items) + f"</{tag}>")
            elif line.startswith("|"):
                flush()
                rows = []
                while i < n and lines[i].startswith("|"):
                    rows.append([c.strip() for c in lines[i].strip().strip("|").split("|")])
                    i += 1
                head, body = rows[0], [r for r in rows[1:] if not all(re.fullmatch(r":?-+:?", c) for c in r)]
                num = lambda c: bool(re.fullmatch(r"[+−-]?[\d.,]+\s?%?(\s?\(.*\))?", c))
                th = "".join(f"<th>{self.inline(c)}</th>" for c in head)
                trs = "".join(
                    "<tr>" + "".join(f'<td{" class=num" if num(c) else ""}>{self.inline(c)}</td>' for c in r) + "</tr>"
                    for r in body)
                out.append(f'<div class="table"><table><thead><tr>{th}</tr></thead><tbody>{trs}</tbody></table></div>')
            elif line.startswith("> "):
                flush()
                quote = []
                while i < n and lines[i].startswith(">"):
                    quote.append(lines[i][1:].strip())
                    i += 1
                out.append(f"<blockquote>{self.render(quote)}</blockquote>")
            else:
                para.append(line.strip())
                i += 1
        flush()
        return "\n".join(out)


def tiles(word: str, cls: str = "") -> str:
    out = []
    for ch in word:
        if ch == " ":
            out.append('<span class="tgap"></span>')
        else:
            out.append(f'<span class="wtile{cls}">{esc(ch)}<i>{TILE_POINTS.get(ch.upper(), "")}</i></span>')
    return "".join(out)


# ---------- templates ----------

HEAD_SCRIPT = (
    "(function(){try{var s=JSON.parse(localStorage.getItem('kingbble-settings')||'{}');"
    "if(s.theme==='light'||s.theme==='dark')document.documentElement.dataset.theme=s.theme;}catch(e){}})();"
)


def layout(page: Page, pages: dict, main: str, body_class: str = "") -> str:
    t = UI[page.lang]
    other = "es" if page.lang == "en" else "en"
    index_items = []
    number = 0
    for key, names, slugs in PARTS:
        lis = []
        for slug in slugs:
            number += 1
            p = pages[(slug, page.lang)]
            cur = ' aria-current="page"' if slug == page.slug else ""
            lis.append(f'<li><a href="{slug}.html"{cur}><span class="n">{number:02d}</span>{esc(p.nav)}</a></li>')
        index_items.append(f'<li class="part"><span>{esc(names[page.lang])}</span><ol>{"".join(lis)}</ol></li>')
    desc = esc(page.lead or t["tagline"])
    return f"""<!doctype html>
<html lang="{page.lang}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>{esc(page.title)} · Kingbble</title>
<meta name="description" content="{desc}">
<link rel="alternate" hreflang="{other}" href="../{other}/{page.slug}.html">
<link rel="icon" href="../assets/logo.svg" type="image/svg+xml">
<link rel="preload" href="../assets/fonts/young-serif-latin-400-normal.woff2" as="font" type="font/woff2" crossorigin>
<script>{HEAD_SCRIPT}</script>
<link rel="stylesheet" href="../assets/site.css">
</head>
<body class="{body_class}">
<a class="skip" href="#text">{t["skip"]}</a>
<header class="top">
  <a class="brand" href="index.html"><img src="../assets/logo.svg" alt="" width="24" height="26"><span>Kingbble</span></a>
  <nav class="top-nav" aria-label="Kingbble">
    <a href="idea.html"{' aria-current="true"' if page.slug not in ("index", "play", "download") else ""}>{t["manual"]}</a>
    <a href="play.html"{' aria-current="true"' if page.slug == "play" else ""}>{t["demo_short"]}</a>
    <a href="download.html"{' aria-current="true"' if page.slug == "download" else ""}>{t["download"]}</a>
  </nav>
  <div class="top-tools">
    <a class="lang" href="../{other}/{page.slug}.html" lang="{other}" hreflang="{other}" title="{t["other_title"]}">{t["other"]}</a>
    <button class="icon" id="theme" type="button" aria-label="{t["theme"]}" title="{t["theme"]}"><svg viewBox="0 0 20 20" aria-hidden="true"><circle cx="10" cy="10" r="6.2" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M10 3.8a6.2 6.2 0 0 0 0 12.4z" fill="currentColor"/></svg></button>
    <button class="icon" id="menu" type="button" aria-label="{t["menu"]}" aria-expanded="false" aria-controls="index"><svg viewBox="0 0 20 20" aria-hidden="true"><path d="M3 5.5h14M3 10h14M3 14.5h14" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"/></svg></button>
  </div>
</header>
<div class="shell">
<nav class="index" id="index" aria-label="{t["contents"]}"><ol>{"".join(index_items)}</ol></nav>
{main}
</div>
<footer class="foot">
  <div class="foot-in">
    <div class="foot-main">
      <a class="foot-brand" href="index.html" aria-label="Kingbble">{tiles("KINGBBLE", " sm")}</a>
      <p class="foot-tag">{t["tagline"]}.</p>
    </div>
    <nav class="foot-links" aria-label="{t["source"]}">
      <a href="{REPO_URL}">{t["source"]}</a>
      <a href="about.html">{esc(pages[("about", page.lang)].nav)}</a>
      <a href="../{other}/{page.slug}.html" lang="{other}" hreflang="{other}">{t["other"]}</a>
    </nav>
    <div class="foot-small">
      <p>{t["foot"]}</p>
      <p>{t["colophon"]}</p>
    </div>
  </div>
</footer>
<script src="../assets/site.js" defer></script>
</body>
</html>
"""


def chapter(page: Page, pages: dict) -> str:
    t = UI[page.lang]
    body = page.render(page.body.splitlines())
    k = ORDER.index(page.slug)
    part = next(names[page.lang] for _, names, slugs in PARTS if page.slug in slugs)
    pager = []
    if k > 0:
        p = pages[(ORDER[k - 1], page.lang)]
        pager.append(f'<a class="prev" href="{p.slug}.html"><small>{t["prev"]}</small>{esc(p.nav)}</a>')
    else:
        pager.append("<span></span>")
    if k + 1 < len(ORDER):
        p = pages[(ORDER[k + 1], page.lang)]
        pager.append(f'<a class="next" href="{p.slug}.html"><small>{t["next"]}</small>{esc(p.nav)}</a>')
    toc = ""
    if len(page.toc) > 1:
        items = "".join(f'<li><a href="#{a}">{esc(text)}</a></li>' for a, text in page.toc)
        toc = f'<nav class="toc" aria-label="{t["on_page"]}"><p>{t["on_page"]}</p><ol>{items}</ol></nav>'
    letter = re.sub(r"[^A-Za-z]", "", slugify(page.nav))[:1].upper() or "K"
    lead = f'<p class="lead">{page.inline(page.lead)}</p>' if page.lead else ""
    main = f"""<main id="text">
<article>
<header class="chapter-head">
  <span class="ctile" aria-hidden="true">{letter}<i>{k + 1}</i></span>
  <p class="kicker">{esc(part)} · {t["chapter"]} {k + 1}</p>
  <h1>{esc(page.title)}</h1>
  {lead}
</header>
{body}
<nav class="pager" aria-label="{t["prev"]} / {t["next"]}">{"".join(pager)}</nav>
</article>
</main>
{toc}"""
    return layout(page, pages, main)


def home(page: Page, pages: dict) -> str:
    t = UI[page.lang]
    body = page.render(page.body.splitlines())
    rows = []
    number = 0
    for key, names, slugs in PARTS:
        items = []
        for slug in slugs:
            number += 1
            if slug == "index":
                continue
            p = pages[(slug, page.lang)]
            items.append(f'<li><a href="{slug}.html"><span class="n">{number:02d}</span><b>{esc(p.nav)}</b>'
                         f'<span class="d">{page.inline(p.meta.get("blurb", p.lead))}</span></a></li>')
        rows.append(f'<section class="toc-part"><h3>{esc(names[page.lang])}</h3><ol>{"".join(items)}</ol></section>')
    m = page.meta
    main = f"""<main id="text" class="home">
<section class="hero">
  <div class="hero-drift" aria-hidden="true"></div>
  <div class="hero-in">
    <p class="hero-word" role="img" aria-label="Kingbble">{tiles("KINGBBLE", " big")}</p>
    <h1>{esc(m["headline"])}</h1>
    <p class="hero-lead">{page.inline(page.lead)}</p>
    <p class="hero-actions">
      <a class="tilebtn" href="../demo/index.html?lang={page.lang}"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M5 3.2v9.6L12.6 8z"/></svg><span>{t["demo"]}</span></a>
      <a class="ghostbtn" href="download.html">{t["download"]}</a>
      <a class="ghostbtn" href="idea.html">{esc(m["read"])}</a>
    </p>
  </div>
</section>
<article class="home-body">
{body}
<section class="home-toc" aria-labelledby="toc-h">
<h2 id="toc-h">{esc(m["toc"])}</h2>
<div class="toc-grid">{"".join(rows)}</div>
</section>
</article>
</main>"""
    return layout(page, pages, main, "is-home")


def redirect_page() -> str:
    return """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Kingbble</title>
<meta name="description" content="A crossword-game engine that tries to guess your rack. Un motor de palabras cruzadas que intenta adivinar tu atril.">
<link rel="icon" href="assets/logo.svg" type="image/svg+xml">
<script>
(function () {
  var lang = "en";
  try {
    var s = JSON.parse(localStorage.getItem("kingbble-settings") || "{}");
    lang = s.lang === "es" || s.lang === "en" ? s.lang : ((navigator.language || "en").slice(0, 2) === "es" ? "es" : "en");
  } catch (e) {}
  location.replace(lang + "/index.html" + location.hash);
})();
</script>
<link rel="stylesheet" href="assets/site.css">
</head>
<body class="gate">
<main class="gate-main">
  <img src="assets/logo.svg" alt="" width="96" height="104">
  <h1>Kingbble</h1>
  <p><a href="en/index.html" lang="en">English</a> · <a href="es/index.html" lang="es">Español</a></p>
</main>
</body>
</html>
"""


def not_found() -> str:
    return """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>404 · Kingbble</title>
<style>
body{margin:0;min-height:100vh;display:grid;place-items:center;background:#f8fafd;color:#1b2030;font:16px/1.5 system-ui,sans-serif;text-align:center;padding:24px}
@media (prefers-color-scheme:dark){body{background:#12151c;color:#e8eaf0}a{color:#e2b866}}
h1{font:400 34px/1.1 Georgia,serif;margin:0 0 8px}p{margin:6px 0}a{color:#7a5a16}
.t{display:inline-grid;place-items:center;width:64px;height:64px;border-radius:5px;background:#f3ead6;color:#2a2418;font:400 40px/1 Georgia,serif;box-shadow:inset 0 -4px 0 #d6c8a6;margin-bottom:18px}
</style>
</head>
<body>
<main>
  <div class="t" aria-hidden="true">?</div>
  <h1>This square is empty · Esta casilla está vacía</h1>
  <p>There is no page at this address. No hay ninguna página en esta dirección.</p>
  <p id="links"><a href="/">Kingbble</a></p>
</main>
<script>
// The site may live under a base path (a project page): find it from this address.
(function () {
  var parts = location.pathname.split("/").filter(Boolean);
  var base = "/";
  var i = parts.findIndex(function (p) { return p === "en" || p === "es" || p === "demo" || p === "assets"; });
  if (i > 0) base = "/" + parts.slice(0, i).join("/") + "/";
  else if (i < 0 && parts.length > 1) base = "/" + parts[0] + "/";
  document.getElementById("links").innerHTML =
    '<a href="' + base + 'en/index.html">English</a> · <a href="' + base + 'es/index.html">Español</a>';
})();
</script>
</body>
</html>
"""


# ---------- demo ----------

def build_demo(out: Path) -> None:
    ui = ROOT / "ui" / "static"
    demo = out / "demo"
    shutil.copytree(ui, demo, dirs_exist_ok=True)
    for f in (WEB / "static" / "demo").iterdir():
        shutil.copy2(f, demo / f.name)
    for name in ("ENABLE.txt", "ENABLE-leaves.txt"):
        src = WEB / "data" / name
        if not src.is_file():
            raise SystemExit(f"missing {src} (see web/README.md)")
        shutil.copy2(src, demo / name)
    for name in ("engine.js", "engine.wasm"):
        if not (demo / name).is_file():
            raise SystemExit(f"missing web/static/demo/{name}: run web/wasm/build.sh")
    # The demo's thinking time stops at 5 seconds.
    app = (demo / "app.js").read_text(encoding="utf-8")
    app, count = re.subn(r"const STEPS = \[[^\]]*\];", "const STEPS = [0.5, 1, 2, 3, 5];", app)
    if count != 1:
        raise SystemExit("ui/static/app.js: STEPS not found")
    (demo / "app.js").write_text(app, encoding="utf-8")
    page = (demo / "index.html").read_text(encoding="utf-8")
    page = page.replace('<link rel="stylesheet" href="style.css">',
                        '<link rel="stylesheet" href="style.css">\n<link rel="stylesheet" href="demo.css">')
    page = page.replace('<script src="app.js"></script>', '<script src="shim.js"></script>\n<script src="app.js"></script>')
    page = page.replace("<title>Kingbble</title>",
                        '<title>Kingbble · demo</title>\n<meta name="description" content="Play against the Kingbble engine '
                        'in the browser. Juega contra el motor Kingbble en el navegador.">')
    if "shim.js" not in page or "demo.css" not in page:
        raise SystemExit("ui/static/index.html: could not add the demo files")
    (demo / "index.html").write_text(page, encoding="utf-8")


def build_images(out: Path) -> None:
    from PIL import Image

    dst = out / "assets" / "img"
    dst.mkdir(parents=True, exist_ok=True)
    for src in sorted((WEB / "static" / "img").iterdir()):
        if src.suffix.lower() == ".png":
            im = Image.open(src).convert("RGB")
            if im.width > 1600:
                im = im.resize((1600, round(im.height * 1600 / im.width)), Image.LANCZOS)
            im.save(dst / (src.stem + ".webp"), "WEBP", quality=82, method=6)
        else:
            shutil.copy2(src, dst / src.name)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(WEB / "dist"))
    ap.add_argument("--no-demo", action="store_true", help="skip the playable demo (pages only)")
    args = ap.parse_args()
    out = Path(args.out)
    if out.exists():
        shutil.rmtree(out)
    (out / "assets" / "fonts").mkdir(parents=True)
    pages = {}
    for slug in ORDER:
        for lang in LANGS:
            path = WEB / "content" / f"{slug}.{lang}.md"
            if not path.is_file():
                raise SystemExit(f"missing {path}")
            pages[(slug, lang)] = Page(slug, lang, path)
    for lang in LANGS:
        (out / lang).mkdir()
        for slug in ORDER:
            page = pages[(slug, lang)]
            text = home(page, pages) if slug == "index" else chapter(page, pages)
            (out / lang / f"{slug}.html").write_text(text, encoding="utf-8")
    (out / "index.html").write_text(redirect_page(), encoding="utf-8")
    (out / "404.html").write_text(not_found(), encoding="utf-8")
    (out / ".nojekyll").write_text("", encoding="utf-8")
    for name in ("site.css", "site.js"):
        shutil.copy2(WEB / "static" / name, out / "assets" / name)
    shutil.copy2(ROOT / "ui" / "static" / "logo.svg", out / "assets" / "logo.svg")
    for f in (ROOT / "ui" / "static" / "fonts").iterdir():
        shutil.copy2(f, out / "assets" / "fonts" / f.name)
    build_images(out)
    if not args.no_demo:
        build_demo(out)
    words = sum(len(re.sub(r"<[^>]+>", " ", (out / lang / f"{s}.html").read_text(encoding="utf-8")).split())
                for lang in LANGS for s in ORDER)
    print(f"{len(ORDER)} pages x {len(LANGS)} languages, about {words} words -> {out}")


if __name__ == "__main__":
    main()
