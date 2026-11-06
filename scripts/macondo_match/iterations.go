// Copied into Macondo's ai/bot package with the match helper, which cannot reach
// the simulator from its own package.
package bot

// SimIterations is the number of iterations of the last simulation, 0 if none ran.
func (p *BotTurnPlayer) SimIterations() int {
	if p.simmer == nil || !p.simmer.Ready() {
		return 0
	}
	return p.simmer.Iterations()
}
