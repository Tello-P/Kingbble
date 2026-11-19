title: About, licence and credits
nav: About and licence
blurb: Who made it, under which licence, what it includes, what it leaves out, and whom it owes.
lead: Kingbble is a one-person project that grew into an engine, an app and this manual. This page says under which terms you may use it, what is and is not included, and whose work it stands on.
---
## The project

Kingbble began with an observation: nobody beats a good chess engine any more, but in this game a strong player still beats the best engines often, because the tiles are hidden and the draw decides a lot. That led to one question: can an engine play better by modelling how *people* choose their moves, and using that to guess the tiles they hold? Answering it meant building everything underneath first, and measuring each part against the engines the community trusts.

It is the work of one person, [Tello-P](https://github.com/Tello-P) on GitHub, developed in the open: every change went through an issue, a branch and a pull request with tests.

- **Source code**: [github.com/Tello-P/Kingbble](repo)
- **Questions and bugs**: the issues of the repository
- **History**: the changelog in the repository, milestone by milestone

## The licence

Kingbble is free software under the **GNU General Public License, version 3**.

In short, and without replacing the text of the licence:

- You may **use** it for any purpose.
- You may **study** it and **change** it.
- You may **share** it, changed or not.
- If you distribute it, or a program that includes it, you must do so **under the same licence**, with the source code, and keep the notices.
- It comes **with no warranty**.

The full text is the `LICENSE` file of the repository and is included in every download.

The content of this site, its text and its figures, is published under the same terms as the code it documents.

## What is included

| Item | Origin | Terms |
|---|---|---|
| Kingbble's code: engine, interface, tools, this site | this project | GPL-3.0 |
| ENABLE word list | Alan Beale and others | public domain |
| Leave table for ENABLE | trained from scratch by this project, by self-play | GPL-3.0, with the code |
| Young Serif typeface | Bastien Sozeau | SIL Open Font License 1.1 |
| IBM Plex Sans typeface | IBM | SIL Open Font License 1.1 |
| Electron, in the desktop app | OpenJS Foundation and contributors | MIT |
| Test word list and leaves (`lexica/fixtures`) | made for this project | GPL-3.0 |

## What is left out, and why

| Item | Why it is not included |
|---|---|
| Tournament word lists (Collins Scrabble Words, the NASPA Word List) | they are licensed by their owners |
| Leave tables published by other projects | their redistribution terms are not confirmed |
| The human move model used in the experiments | it was fitted on games from an online platform; whether it may be redistributed is an open question |
| The human games themselves | they belong to their players and the platform; the project keeps them pseudonymised and does not publish them |
| A Spanish word list | none is known that may be freely distributed |

The experiments were measured on a tournament list under local use, cited and not redistributed. Everything a visitor can download from here is built only from the first table.

## Trademarks and affiliation

Kingbble is an independent project. **It is not affiliated with, sponsored by or endorsed by Hasbro, Mattel, or any owner of crossword-game trademarks.** SCRABBLE® is a registered trademark of its respective owners: Hasbro, Inc. in the United States and Canada, and Mattel, Inc. elsewhere. The names of word lists and of other engines appear on this site only to describe what Kingbble was compared with and is compatible with.

Kingbble's name, logo and visual design are its own.

## Standing on others' work

The code is Kingbble's own. The ideas are not, and the project could not exist without those who published theirs.

| | What Kingbble owes |
|---|---|
| **Andrew Appel and Guy Jacobson** | the algorithm for generating moves from a word graph with anchors and cross-checks (1988) |
| **Steven Gordon** | the GADDAG, which lets moves be built outwards from any letter (1994) |
| **Brian Sheppard** (Maven) | simulation, leave values and the exact endgame as the architecture of a strong engine |
| **Quackle** (Jason Katz-Brown, John O'Laughlin and contributors) | the reference for two decades, and RangeFinder, the baseline of the inference |
| **Macondo** (César Del Solar and contributors) | the reference Kingbble is checked against position by position, and the opponent of its matches |
| **wolges** (Andy Kurnia) | the KWG and KLV formats, and the full-rack method for leave values |
| **Magpie** | the same leave method, and ideas for a fast engine |
| **Woogles** | the open platform whose public games made a model of human play possible |
| **The players** | every recorded game is hours of someone's thought |

Kingbble communicates with Quackle and Macondo only as separate programs, for comparison. None of their code is included or linked.

## Privacy

- **This site** has no analytics, no cookies and no third-party scripts. The typefaces are served from the site itself.
- **The web demo** runs in your browser. Nothing you do in it is sent anywhere.
- **The desktop app** makes no connections.
- The only thing stored is your **settings**, on your own device.
- If the site is hosted on GitHub Pages, GitHub, as the host, may log the addresses of visitors under its own privacy terms.

## How this site is made

Plain HTML and CSS, generated by one Python script from text files, with a few lines of JavaScript for the theme, the menu and the contents. No framework, no tracker, no external request. It is set in Young Serif and IBM Plex Sans, the same typefaces and colours as the app, so that the manual and the game are visibly the same thing.

The source of every page is in the `web/` folder of the repository. Corrections are welcome as issues or pull requests.

## Citing

If you use Kingbble in published work, cite the repository and the version:

```
Kingbble: a crossword-game engine with opponent-rack inference
calibrated on human play. Version 1.0, 2026.
https://github.com/Tello-P/Kingbble
```
