// Position bank: RiskBot against SimBot from mid and late game positions, where
// risk matters. Position i comes from equity self-play stopped at a bag of
// --min-bag..--max-bag tiles (drawn per position) with the player to move at
// most --max-lead points ahead or behind. Each position is played to the end
// --repeats times (each with its own bag order), twice per repeat: a (the risk
// bot) on the side to move, then b (the sim bot) on it. Both play "+peg+eg"
// (pre-endgame with 1 tile in the bag, endgame with none), and each tracks the
// other's rack from the start of the game with a RackFilter on --model
// (rangefinder by default; none for uniform racks; human with --human-params).
//   kingbble_position_bank --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2
//       --positions 30 --repeats 1 [--a risk|riskwin] [--risk-lambda 1] [--seed 1]
//       [--sim-candidates 8] [--sim-iterations 50] [--threads 3] [--peg-time 2]
//       [--endgame-time 1] [--model rangefinder] [--human-params FILE]
//       [--results FILE [--status]] [--progress]
// Results use kingbble_selfplay's format ("game N: S1 - S2, a in seat K", the
// two games of a pair numbered 2k-1 and 2k), so a rerun skips finished games.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "results_file.hpp"
#include "scrabble/endgame_bot.hpp"
#include "scrabble/equity.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/game.hpp"
#include "scrabble/inference.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/preendgame_bot.hpp"
#include "scrabble/risk_bot.hpp"
#include "scrabble/sim_bot.hpp"

using namespace scrabble;

namespace {

std::uint64_t MixSeed(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

// Every rack explains every move: the posterior is the prior.
class UniformModel : public MoveModel {
 public:
  double Likelihood(const Observation&, const Rack&) override { return 1; }
};

struct Options {
  int positions = 30, repeats = 1, min_bag = 10, max_bag = 25, max_lead = 80;
  std::uint64_t seed = 1;
  std::string a = "risk";
  SimParams sim;
  RiskParams risk;
  double peg_time = 2, endgame_time = 1;
};

// A position: the seed of the equity game it comes from and its moves.
struct Position {
  std::uint64_t game_seed = 0;
  std::vector<Move> moves;
};

Position FindPosition(const Options& o, EquityBot& equity, int index) {
  std::mt19937_64 rng(MixSeed(o.seed * 7919 + static_cast<std::uint64_t>(index)));
  for (std::uint64_t attempt = 0;; ++attempt) {
    Position p;
    p.game_seed = MixSeed(rng());
    const int target = std::uniform_int_distribution<int>(o.min_bag, o.max_bag)(rng);
    Game g(p.game_seed);
    while (!g.over() && g.bag_size() > target) {
      p.moves.push_back(equity.Choose(g));
      g.Apply(p.moves.back());
    }
    const int me = g.to_move();
    if (!g.over() && g.bag_size() >= o.min_bag &&
        std::abs(g.score(me) - g.score(1 - me)) <= o.max_lead)
      return p;
  }
}

// One side: its bot, the sim or risk bot inside it and its tracker of the
// other side's rack.
struct Side {
  std::unique_ptr<Bot> bot;
  SimBot* sim = nullptr;
  RiskBot* risk = nullptr;
  std::unique_ptr<RackFilter> filter;
};

// Plays the game to the end, updating both trackers as inference_eval does.
class Table {
 public:
  explicit Table(Side* const seats[2]) : seats_{seats[0], seats[1]} {}

  void Start(const Game& game) {
    for (int s = 0; s < 2; ++s)
      if (auto& f = seats_[s]->filter)
        f->Reset(UnseenTiles(game.board(), game.rack(s)), game.rack(1 - s).Size());
  }

  void Play(Game& game, const Move& m) {
    const int me = game.to_move(), opp = 1 - me;
    Observation obs;
    obs.board = game.board();
    obs.move = m;
    obs.bag_size = game.bag_size();
    obs.own = game.rack(opp);
    obs.unseen = UnseenTiles(game.board(), game.rack(opp));
    obs.score_diff = game.score(me) - game.score(opp);
    obs.mover_is_bot = true;
    obs.turn = turn_++;
    obs.prev_exchange = exchanged_[me];
    exchanged_[me] = m.kind == Move::Kind::kExchange;
    if (m.kind == Move::Kind::kExchange) {
      obs.exchanged = static_cast<int>(m.tiles.size());
      obs.move.tiles.clear();
    }
    game.Apply(m);
    if (game.over()) return;
    if (auto& f = seats_[opp]->filter) f->ObserveOpponent(obs);
    if (auto& f = seats_[me]->filter)
      f->Resync(UnseenTiles(game.board(), game.rack(me)), game.rack(opp).Size());
  }

 private:
  Side* seats_[2];
  int turn_ = 0;
  bool exchanged_[2] = {false, false};
};

}  // namespace

int main(int argc, char** argv) {
  Options o;
  o.sim.candidates = 8;
  o.sim.max_iterations = 50;
  std::string lexicon, leaves_path, model_name = "rangefinder", human_path, results_path;
  bool progress = false, status_only = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto value = [&]() -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "missing value for " << a << "\n";
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--lexicon") {
      lexicon = value();
    } else if (a == "--leaves") {
      leaves_path = value();
    } else if (a == "--positions") {
      o.positions = std::stoi(value());
    } else if (a == "--repeats") {
      o.repeats = std::stoi(value());
    } else if (a == "--seed") {
      o.seed = std::stoull(value());
    } else if (a == "--a") {
      o.a = value();
    } else if (a == "--risk-lambda") {
      o.risk.lambda_scale = std::stod(value());
    } else if (a == "--sim-candidates") {
      o.sim.candidates = std::stoi(value());
    } else if (a == "--sim-iterations") {
      o.sim.max_iterations = std::stoi(value());
    } else if (a == "--threads") {
      o.sim.threads = std::stoi(value());
    } else if (a == "--peg-time") {
      o.peg_time = std::stod(value());
    } else if (a == "--endgame-time") {
      o.endgame_time = std::stod(value());
    } else if (a == "--model") {
      model_name = value();
    } else if (a == "--human-params") {
      human_path = value();
    } else if (a == "--results") {
      results_path = value();
    } else if (a == "--status") {
      status_only = true;
    } else if (a == "--progress") {
      progress = true;
    } else {
      std::cerr << "usage: " << argv[0]
                << " --lexicon FILE --leaves KLV [--positions N] [--repeats N] [--a risk|riskwin]"
                   " [--risk-lambda L] [--seed S] [--sim-candidates N] [--sim-iterations N]"
                   " [--threads N] [--peg-time S] [--endgame-time S] [--model NAME]"
                   " [--human-params FILE] [--results FILE [--status]] [--progress]\n";
      return 2;
    }
  }
  const int games = 2 * o.positions * o.repeats;
  if (status_only) {
    ResultsFile results(results_path, true);
    int done = 0;
    for (int g = 1; g <= games; ++g) done += results.Done(g);
    std::cout << "games: " << done << " of " << games << "\n";
    return 0;
  }
  if (lexicon.empty() || leaves_path.empty() || (o.a != "risk" && o.a != "riskwin")) {
    std::cerr << "--lexicon and --leaves are required; --a is risk or riskwin\n";
    return 2;
  }
  Gaddag gaddag = Gaddag::FromFile(lexicon);
  Leaves leaves = Leaves::FromFile(leaves_path);
  EquityBot equity(gaddag, leaves);
  o.sim.seed = o.seed;
  o.risk.mode = o.a == "risk" ? RiskMode::kMeanVariance : RiskMode::kWinProb;

  std::unique_ptr<MoveModel> models[2];
  for (auto& m : models) {
    if (model_name == "none") {
      m = std::make_unique<UniformModel>();
    } else if (model_name == "rangefinder") {
      m = std::make_unique<RangeFinderModel>(gaddag, leaves);
    } else if (model_name == "human" && !human_path.empty()) {
      m = std::make_unique<HumanMoveModel>(gaddag, leaves, HumanModelParams::FromFile(human_path));
    } else {
      std::cerr << "--model is none, rangefinder or human (with --human-params)\n";
      return 2;
    }
  }
  PreEndgameParams peg;
  peg.candidates = 10;
  peg.time_limit_s = o.peg_time;
  // sides[0] is a (risk), sides[1] is b (sim).
  Side sides[2];
  for (int s = 0; s < 2; ++s) {
    std::unique_ptr<Bot> inner;
    if (s == 0) {
      auto risk = std::make_unique<RiskBot>(gaddag, leaves, o.sim, o.risk);
      sides[s].risk = risk.get();
      inner = std::move(risk);
    } else {
      auto sim = std::make_unique<SimBot>(gaddag, leaves, o.sim);
      sides[s].sim = sim.get();
      inner = std::move(sim);
    }
    inner = std::make_unique<PreEndgameBot>(std::move(inner), gaddag, leaves, 1, peg);
    sides[s].bot =
        std::make_unique<EndgameBot>(std::move(inner), gaddag, o.endgame_time, o.sim.threads);
  }

  ResultsFile results(results_path);
  int done = 0;
  for (int g = 1; g <= games; ++g) done += results.Done(g);
  if (progress) std::cerr << "progress: " << done << "/" << games << std::endl;
  for (int i = 0; i < o.positions; ++i) {
    bool needed = false;
    for (int k = 0; k < 2 * o.repeats; ++k) needed |= !results.Done(2 * i * o.repeats + k + 1);
    if (!needed) continue;
    const Position pos = FindPosition(o, equity, i);
    for (int r = 0; r < o.repeats; ++r) {
      for (int s = 0; s < 2; ++s) {
        const int number = 2 * (i * o.repeats + r) + s + 1;
        if (results.Done(number)) continue;
        // Fresh trackers, fed the whole game from its start.
        for (int k = 0; k < 2; ++k) {
          FilterParams fp;
          fp.seed = MixSeed(pos.game_seed + 10 + k);
          sides[k].filter = std::make_unique<RackFilter>(*models[k], leaves, fp);
          if (model_name == "none") sides[k].filter.reset();
          if (sides[k].risk) sides[k].risk->set_rack_sampler(sides[k].filter.get());
          if (sides[k].sim) sides[k].sim->set_rack_sampler(sides[k].filter.get());
        }
        Game game(pos.game_seed);
        const int to_move_at = [&] {
          Game g(pos.game_seed);
          for (const Move& m : pos.moves) g.Apply(m);
          return g.to_move();
        }();
        // a on the side to move first, then b.
        const int seat_a = s == 0 ? to_move_at : 1 - to_move_at;
        Side* seats[2] = {&sides[seat_a == 0 ? 0 : 1], &sides[seat_a == 0 ? 1 : 0]};
        Table table(seats);
        table.Start(game);
        for (const Move& m : pos.moves) table.Play(game, m);
        // Its own bag order per repeat, the same for both games of a pair.
        const int me = game.to_move();
        const Rack bag = MinusTiles(UnseenTiles(game.board(), game.rack(me)), game.rack(1 - me));
        game = Game::FromPosition(game.board(), {game.rack(0), game.rack(1)},
                                  {game.score(0), game.score(1)}, me, bag, game.scoreless_turns(),
                                  MixSeed(pos.game_seed ^ (0xBA6ULL + r)));
        while (!game.over()) table.Play(game, seats[game.to_move()]->bot->Choose(game));
        results.Append(number, "game " + std::to_string(number) + ": " +
                                   std::to_string(game.score(0)) + " - " +
                                   std::to_string(game.score(1)) + ", a in seat " +
                                   std::to_string(seat_a + 1));
        if (progress) std::cerr << "progress: " << ++done << "/" << games << std::endl;
      }
    }
  }
  int wins = 0, n = 0;
  long long spread = 0;
  for (const std::string& line : results.lines()) {
    int g = 0, s0 = 0, s1 = 0, seat = 0;
    if (std::sscanf(line.c_str(), "game %d: %d - %d, a in seat %d", &g, &s0, &s1, &seat) != 4 ||
        g > games)
      continue;
    const int sa = seat == 1 ? s0 : s1, sb = seat == 1 ? s1 : s0;
    wins += 2 * (sa > sb) + (sa == sb);
    spread += sa - sb;
    ++n;
  }
  std::cout << "a=" << o.a << "+peg+eg vs b=sim+peg+eg over " << n
            << " games: a win %: " << (n ? 50.0 * wins / n : 0)
            << ", a avg spread: " << (n ? double(spread) / n : 0) << "\n";
  return 0;
}
