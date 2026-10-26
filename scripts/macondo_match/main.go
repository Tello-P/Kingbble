// Plays Macondo's side of a match: reads CGP positions on stdin (the player to
// move's rack only, the opponent's left empty so it is not seen) and prints the
// move Macondo's bot picks within -time seconds as "MOVE <coords> <tiles>"
// ('.' for played-through squares, lowercase for blanks), "MOVE -<tiles>" for
// an exchange or "MOVE -" for a pass. The bot simulates, solves pre-endgames
// and endgames (SIMMING_BOT). Built inside a Macondo checkout by
// scripts/issues/issue_58_macondo_match.sh.
package main

import (
	"bufio"
	"context"
	"flag"
	"fmt"
	"os"
	"time"

	"github.com/rs/zerolog"

	"github.com/domino14/macondo/ai/bot"
	"github.com/domino14/macondo/cgp"
	"github.com/domino14/macondo/config"
	pb "github.com/domino14/macondo/gen/api/proto/macondo"
	"github.com/domino14/macondo/move"
)

func main() {
	dataPath := flag.String("data", "", "Macondo data directory (lexica/gaddag/<lex>.kwg and .klv2 inside)")
	seconds := flag.Float64("time", 5, "seconds per move")
	threads := flag.Int("threads", 1, "simulation threads")
	flag.Parse()
	zerolog.SetGlobalLevel(zerolog.WarnLevel)

	cfg := config.DefaultConfig()
	cfg.Set(config.ConfigDataPath, *dataPath)

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
		conf := &bot.BotConfig{Config: *cfg, SimThreads: *threads}
		p, err := bot.NewBotTurnPlayerFromGame(g, conf, pb.BotRequest_SIMMING_BOT)
		if err != nil {
			fmt.Fprintln(os.Stderr, "bot:", err)
			os.Exit(1)
		}
		ctx, cancel := context.WithTimeout(context.Background(),
			time.Duration(*seconds*float64(time.Second)))
		m, err := p.BestPlay(ctx)
		cancel()
		if err != nil || m == nil {
			// Out of time before any answer: the best static play.
			fmt.Fprintln(os.Stderr, "best play:", err, "- using the static best")
			m = p.GenerateMoves(1)[0]
		}
		switch m.Action() {
		case move.MoveTypePlay:
			fmt.Fprintf(out, "MOVE %s %s\n", m.BoardCoords(), m.TilesString())
		case move.MoveTypeExchange:
			fmt.Fprintf(out, "MOVE -%s\n", m.TilesStringExchange())
		default:
			fmt.Fprintln(out, "MOVE -")
		}
		out.Flush()
	}
	if err := in.Err(); err != nil {
		fmt.Fprintln(os.Stderr, "reading stdin:", err)
		os.Exit(1)
	}
}
