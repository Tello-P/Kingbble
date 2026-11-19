title: The five levels
nav: Levels
blurb: From the highest score to rack inference: what each level computes and how it feels to play.
lead: A level is not a handicap applied to one bot. Each level is a different way of choosing a move, with one more part of the engine switched on than the level before.
---
:::raw
<div class="levels">
  <div class="level"><span class="ctile" aria-hidden="true">E<i>1</i></span><div><h3>Easy</h3><p>Plays the move with the highest score. Engine name: <code>greedy</code>.</p></div><span class="where">Web and desktop</span></div>
  <div class="level"><span class="ctile" aria-hidden="true">M<i>3</i></span><div><h3>Medium</h3><p>Score plus the value of the tiles it keeps. Engine name: <code>equity</code>.</p></div><span class="where">Web and desktop</span></div>
  <div class="level"><span class="ctile" aria-hidden="true">S<i>1</i></span><div><h3>Strong</h3><p>Simulates what happens after each of its best candidates. Engine name: <code>sim</code>.</p></div><span class="where">Web and desktop</span></div>
  <div class="level"><span class="ctile" aria-hidden="true">V<i>4</i></span><div><h3>Very strong</h3><p>Simulation, and the exact solution once the bag is empty. Engine name: <code>sim+eg</code>.</p></div><span class="where">Web and desktop</span></div>
  <div class="level locked"><span class="ctile" aria-hidden="true">X<i>8</i></span><div><h3>Extreme</h3><p>All of the above, plus a running estimate of the tiles you hold, on every core of the computer.</p></div><span class="where">Desktop only</span></div>
</div>
:::

## Easy: the highest score

The bot generates every legal move and plays the one worth the most points. Nothing else.

It is fast, it never makes an illegal move, and it finds long words a person would miss, because it sees every one of them. It is also short-sighted: it will spend a blank for four extra points, keep a rack of three I's and a Q, and open a triple word square for you without a second thought. Against it, the way to win is the way the rest of this manual explains: look after your rack and wait for it to ruin its own.

How the moves are found: [the move generator](movegen).

## Medium: static equity

Now the bot adds, to the points of each move, the value of the tiles it would keep. A move that scores 22 and keeps `ERS?` is better than one that scores 30 and keeps `UUVW`, and the leave table knows by how much.

This one change is the largest step in strength of the five. The bot starts playing like someone who knows the game: it holds on to blanks and S's, gets rid of duplicates, and exchanges when its rack is hopeless. It still does not look at the board from your side: it does not know what its move opens for you.

How the values are obtained: [equity and leaves](equity).

## Strong: simulation

The bot takes its best candidates by equity and, for each, plays the game forward many times: it deals you a random rack from the tiles it has not seen, lets you answer, plays its own next move, and notes how the score ended. The candidate with the best average wins.

This is where it begins to defend. A move that leaves a triple word open with the tiles to use it still in the bag comes out with a bad average, and the bot stops playing it, without any rule about triple word squares being written anywhere.

The thinking time you choose is spent here. The simulation stops early when one candidate is clearly ahead, so with an obvious move it answers before the time is up.

How it works: [simulation](simulation).

## Very strong: simulation plus the exact endgame

The same as Strong until the bag is empty. From then on the bot knows your rack exactly, since it is everything it cannot see, and the game becomes one of perfect information. The engine searches it to the end and plays the move that is provably best, or the best it has proven when the time ends.

You notice it in close games. It blocks your last out-play, goes out a turn before you expected, or sticks you with the Q. Measured against static equity, the exact endgame is worth about 4.8 points per game.

How it works: [the endgame](endgame).

## Extreme: it reads your rack

The top level adds the part this project was built for. After each move you make, the engine asks which racks would have made that move a reasonable choice, and keeps a weighted set of the racks you might be holding now. When it simulates, it deals you racks from that set instead of random ones.

If you play a short word for few points and keep five tiles, it supposes you are building toward a seven-tile play, and it starts closing the lanes. If you exchange, it stops fearing your blanks.

It also uses every core of the computer for the simulation and the endgame, and it accepts longer thinking times. That is why it is in the desktop app only: a browser tab has one thread for this, and a level that needs minutes of a phone's only thread would not be a level, it would be a hang.

:::note Which inference the public Extreme uses
In the experiments, the inference uses a model of human play fitted on real games. Whether that model file may be redistributed is an open question, so the public build does not include it. The Extreme level of the public app is assembled only from parts that need no human data, chosen by measuring the candidates against each other on ENABLE. See [the human model](human-model) and [rack inference](inference).
:::

## Thinking time

| | Web demo | Desktop app |
|---|---|---|
| Easy, Medium | instant | instant |
| Strong, Very strong | 0.5, 1, 2, 3 or 5 seconds | 0.5 to 60 seconds |
| Extreme | not available | 0.5 to 60 seconds |

More time means more simulations for each candidate, not a deeper search. The gain flattens quickly: in the project's own measurements, multiplying the iterations by 16 did not win more games (see [what did not work](lessons)). One or two seconds is a good setting. Use more when you want the analysis of one position, not for a whole game.

## Which one to choose

- **To learn the game**: Easy, then Medium. When Medium stops beating you, you have learned to manage a rack.
- **For a real game**: Strong at one or two seconds.
- **To practise endgames**: Very strong. Review the game afterwards and look at the last four moves.
- **To see the idea of the project in action**: Extreme, in the [desktop app](download).

In **Bot vs bot** mode you can put any two levels against each other and watch what the difference between them looks like on the board.
