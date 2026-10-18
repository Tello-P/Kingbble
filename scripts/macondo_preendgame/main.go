// Reads CGP pre-endgame positions (1-6 tiles in the bag, one per line) on stdin
// and prints, for each, one JSON line with Macondo's pre-endgame solution: every
// analysed play, best first, with its wins (ties count half, counted in bag
// permutations) and its tiebreak spread when it has one. A line may end with a
// tab and the plays to analyse, "8D WORD (12)" style or "pass", separated by ';'.
// Built inside a Macondo checkout by scripts/compare_preendgame.py.
package main

import (
	"bufio"
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"strings"

	"github.com/domino14/word-golib/kwg"
	"github.com/rs/zerolog"

	"github.com/domino14/macondo/cgp"
	"github.com/domino14/macondo/config"
	"github.com/domino14/macondo/move"
	"github.com/domino14/macondo/movegen"
	"github.com/domino14/macondo/preendgame"
)

type play struct {
	Move     string  `json:"move"`
	Wins     float32 `json:"wins"`
	Spread   *int    `json:"spread"`
	Ignored  bool    `json:"ignored"`
	Outcomes int     `json:"outcomes"`
}

type solution struct {
	Plays []play `json:"plays"`
}

func describe(m *move.Move) string {
	if m.Action() == move.MoveTypePass {
		return "pass"
	}
	return fmt.Sprintf("%s %s (%d)", m.BoardCoords(), m.TilesString(), m.Score())
}

func main() {
	dataPath := flag.String("data", "", "Macondo data directory (lexica/gaddag/<lex>.kwg inside)")
	lexicon := flag.String("lex", "ENABLE", "lexicon name")
	plies := flag.Int("plies", 25, "endgame search depth")
	threads := flag.Int("threads", 1, "solver threads")
	skipDeepPass := flag.Bool("skip-deep-pass", false, "only pass after a scoreless turn in nested pre-endgames")
	noCutoff := flag.Bool("no-cutoff", false, "analyse every play fully (no early cutoff)")
	flag.Parse()
	zerolog.SetGlobalLevel(zerolog.WarnLevel)

	cfg := config.DefaultConfig()
	cfg.Set(config.ConfigDataPath, *dataPath)
	gd, err := kwg.GetKWG(cfg.WGLConfig(), *lexicon)
	if err != nil {
		fmt.Fprintln(os.Stderr, "loading KWG:", err)
		os.Exit(1)
	}

	in := bufio.NewScanner(os.Stdin)
	in.Buffer(make([]byte, 1<<20), 1<<20)
	out := bufio.NewWriter(os.Stdout)
	defer out.Flush()
	for in.Scan() {
		line := in.Text()
		if line == "" {
			continue
		}
		cgpStr, only, _ := strings.Cut(line, "\t")
		parsed, err := cgp.ParseCGP(cfg, cgpStr)
		if err != nil {
			fmt.Fprintln(os.Stderr, "parsing CGP:", err, line)
			os.Exit(1)
		}
		g := parsed.Game
		g.RecalculateBoard()
		s := new(preendgame.Solver)
		if err := s.Init(g, gd); err != nil {
			fmt.Fprintln(os.Stderr, "init:", err)
			os.Exit(1)
		}
		s.SetThreads(*threads)
		s.SetEndgamePlies(*plies)
		s.SetIterativeDeepening(false)
		s.SetSkipDeepPass(*skipDeepPass)
		s.SetEarlyCutoffOptim(!*noCutoff)
		if only != "" {
			want := map[string]bool{}
			for _, m := range strings.Split(only, ";") {
				want[m] = true
			}
			gen := movegen.NewGordonGenerator(gd, g.Board(), g.Bag().LetterDistribution())
			gen.SetGenPass(true)
			var moves []*move.Move
			for _, m := range gen.GenAll(g.RackFor(g.PlayerOnTurn()), false) {
				if want[describe(m)] {
					c := new(move.Move)
					c.CopyFrom(m)
					moves = append(moves, c)
				}
			}
			if len(moves) != len(want) {
				fmt.Fprintln(os.Stderr, "found", len(moves), "of", len(want), "plays:", line)
				os.Exit(1)
			}
			s.SetSolveOnly(moves)
		}
		plays, err := s.Solve(context.Background())
		if err != nil {
			fmt.Fprintln(os.Stderr, "solve:", err, line)
			os.Exit(1)
		}
		var sol solution
		for _, p := range plays {
			q := play{Move: describe(p.Play), Wins: p.Points, Ignored: p.Ignore, Outcomes: p.TotalOutcomes()}
			if p.HasSpread() {
				v := p.GetSpread()
				q.Spread = &v
			}
			sol.Plays = append(sol.Plays, q)
		}
		b, _ := json.Marshal(sol)
		fmt.Fprintln(out, string(b))
		out.Flush()
	}
	if err := in.Err(); err != nil {
		fmt.Fprintln(os.Stderr, "reading stdin:", err)
		os.Exit(1)
	}
}
