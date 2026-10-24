// Reads CGP endgame positions (empty bag, one per line) on stdin and prints the
// value Macondo's endgame solver finds for the player to move: the spread they
// gain from here to the end with best play, one integer per line.
// Built inside a Macondo checkout by scripts/compare_endgame.py.
package main

import (
	"bufio"
	"context"
	"flag"
	"fmt"
	"os"
	"time"

	"github.com/domino14/word-golib/kwg"

	"github.com/domino14/macondo/cgp"
	"github.com/domino14/macondo/config"
	"github.com/domino14/macondo/endgame/negamax"
	"github.com/domino14/macondo/game"
	"github.com/domino14/macondo/movegen"
)

func main() {
	dataPath := flag.String("data", "", "Macondo data directory (lexica/gaddag/<lex>.kwg inside)")
	lexicon := flag.String("lex", "ENABLE", "lexicon name")
	plies := flag.Int("plies", 25, "maximum search depth")
	threads := flag.Int("threads", 1, "solver threads")
	ttFrac := flag.Float64("ttable-frac", 0.02, "fraction of RAM for the transposition table of each process")
	timeout := flag.Float64("timeout", 0, "seconds per position (0 = none); slower positions print skip")
	flag.Parse()

	cfg := config.DefaultConfig()
	cfg.Set(config.ConfigDataPath, *dataPath)
	cfg.Set(config.ConfigTtableMemFraction, *ttFrac)
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
		g.SetBackupMode(game.SimulationMode)
		gen := movegen.NewGordonGenerator(gd, g.Board(), g.Bag().LetterDistribution())
		s := new(negamax.Solver)
		if err := s.Init(gen, g); err != nil {
			fmt.Fprintln(os.Stderr, "init:", err)
			os.Exit(1)
		}
		s.SetThreads(*threads)
		ctx, cancel := context.WithCancel(context.Background())
		if *timeout > 0 {
			ctx, cancel = context.WithTimeout(context.Background(), time.Duration(*timeout*float64(time.Second)))
		}
		val, _, err := s.Solve(ctx, *plies)
		timedOut := ctx.Err() != nil
		cancel()
		if timedOut {
			fmt.Fprintln(out, "skip")
			out.Flush()
			continue
		}
		if err != nil {
			fmt.Fprintln(os.Stderr, "solve:", err, line)
			os.Exit(1)
		}
		fmt.Fprintln(out, val)
		out.Flush()
	}
	if err := in.Err(); err != nil {
		fmt.Fprintln(os.Stderr, "reading stdin:", err)
		os.Exit(1)
	}
}
