// Reads CGP positions (one per line) on stdin and prints every legal placement
// Macondo generates for the player to move: a count line, then one
// "<coord> <tiles> (<score>)" line per move ('.' marks played-through squares).
// Built inside a Macondo checkout by scripts/compare_macondo.py.
package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"

	"github.com/domino14/word-golib/kwg"

	"github.com/domino14/macondo/cgp"
	"github.com/domino14/macondo/config"
	"github.com/domino14/macondo/move"
	"github.com/domino14/macondo/movegen"
)

func main() {
	dataPath := flag.String("data", "", "Macondo data directory (lexica/gaddag/<lex>.kwg inside)")
	lexicon := flag.String("lex", "ENABLE", "lexicon name")
	flag.Parse()

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
		parsed, err := cgp.ParseCGP(cfg, line)
		if err != nil {
			fmt.Fprintln(os.Stderr, "parsing CGP:", err, line)
			os.Exit(1)
		}
		g := parsed.Game
		g.RecalculateBoard()
		gen := movegen.NewGordonGenerator(gd, g.Board(), g.Bag().LetterDistribution())
		gen.SetSortingParameter(movegen.SortByNone)
		plays := gen.GenAll(g.RackFor(g.PlayerOnTurn()), false)
		n := 0
		for _, m := range plays {
			if m.Action() == move.MoveTypePlay {
				n++
			}
		}
		fmt.Fprintln(out, n)
		for _, m := range plays {
			if m.Action() == move.MoveTypePlay {
				fmt.Fprintf(out, "%s %s (%d)\n", m.BoardCoords(), m.TilesString(), m.Score())
			}
		}
	}
	if err := in.Err(); err != nil {
		fmt.Fprintln(os.Stderr, "reading stdin:", err)
		os.Exit(1)
	}
}
