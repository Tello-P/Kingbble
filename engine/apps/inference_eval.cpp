// Evaluates opponent-rack inference, writing one JSONL record per inference for
// ml/eval/metrics.py (log-loss, accuracy, tile ECE).
//
// Human games: for every opponent move in the GCG games, infers the leave the
// mover kept and scores it against their real rack.
//   kingbble_inference_eval --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2
//       --games data/raw/woogles --turns test_turns.tsv --models none,rangefinder,human
//       --human-params models/human_model.txt --records out.jsonl [--max-turns N] [--progress]
// --turns (optional) lists the turns to score as "game_id<TAB>turn<TAB>rating"
// lines; without it every turn is scored with an unknown rating. Blind
// exchanges and phonies are skipped.
//
// Self-play: two sim bots, each tracking the other's rack with a RackFilter
// built on its model ("none" deals uniform racks only), seats swapped every game.
//   kingbble_inference_eval --lexicon ... --leaves ... --a human --b rangefinder
//       --games-played 100 --seed 1 --records out.jsonl [--b-rare 0.3] [--progress]
// --a-rare / --b-rare P make that side play, with probability P, a uniformly
// random legal option ranked --rare-from or lower by static equity (the
// stress test). Records score each tracked rack before the tracker moves.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "scrabble/equity.hpp"
#include "scrabble/features.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/game.hpp"
#include "scrabble/gcg.hpp"
#include "scrabble/inference.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/sim_bot.hpp"

using namespace scrabble;
namespace fs = std::filesystem;

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

struct Setup {
  const Gaddag* gaddag;
  const Leaves* leaves;
  HumanModelParams human;
  double tau = 0;
};

std::unique_ptr<MoveModel> MakeModel(const std::string& name, const Setup& s) {
  if (name == "none") return std::make_unique<UniformModel>();
  if (name == "rangefinder") return std::make_unique<RangeFinderModel>(*s.gaddag, *s.leaves, s.tau);
  if (name == "human") return std::make_unique<HumanMoveModel>(*s.gaddag, *s.leaves, s.human);
  std::cerr << "unknown model: " << name << " (none, rangefinder, human)\n";
  std::exit(2);
}

std::string JsonEscape(const std::string& s) {
  std::string out;
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out;
}

// One record in the format of ml/eval/metrics.py.
void WriteRecord(std::ostream& out, const std::string& model, const RackPosterior& post,
                 const Rack& truth, const Rack& possible, std::optional<double> p_true = {}) {
  out << "{\"model\":\"" << JsonEscape(model) << "\",\"truth\":\"" << truth.ToString()
      << "\",\"guess\":\"" << (post.empty() ? "" : post.Map().ToString())
      << "\",\"p_true\":" << p_true.value_or(post.Probability(truth)) << ",\"tile_probs\":{";
  bool first = true;
  for (int t = 0; t < kNumTileTypes; ++t) {
    if (possible.Count(Tile(t)) == 0) continue;
    out << (first ? "" : ",") << "\"" << TileToChar(Tile(t)) << "\":" << post.TileProbability(Tile(t));
    first = false;
  }
  out << "}}\n";
}

// Posterior probability of the true leave when InferRack samples leaves (the
// truth is then often missing from the samples): prior(truth) * L(truth) / Z,
// with Z, the mean likelihood under the prior, estimated from fresh draws.
std::optional<double> SampledTruthProbability(const Observation& obs, MoveModel& model,
                                              const InferParams& params, const Rack& truth) {
  const Rack played = MoveTiles(obs.move);
  const Rack pool = MinusTiles(obs.unseen, played);
  if (CountSubRacks(pool, truth.Size(), params.exact_limit + 1) <= params.exact_limit) return {};
  std::mt19937_64 rng(MixSeed(params.seed));
  double z = 0;
  for (int i = 0; i < params.samples; ++i)
    z += model.Likelihood(obs, PlusTiles(DrawTiles(pool, truth.Size(), rng), played));
  z /= params.samples;
  const double prior = DrawProbability(pool, truth);
  // With no rack explaining the move InferRack keeps the prior.
  return z > 0 ? prior * model.Likelihood(obs, PlusTiles(truth, played)) / z : prior;
}

std::vector<std::string> SplitList(const std::string& s) {
  std::vector<std::string> out;
  std::stringstream ss(s);
  for (std::string x; std::getline(ss, x, ',');)
    if (!x.empty()) out.push_back(x);
  return out;
}

struct Options {
  std::string games_dir, turns_path, records_path;
  std::vector<std::string> models{"none", "rangefinder", "human"};
  std::string model_a = "human", model_b = "rangefinder";
  double rare[2] = {0, 0};
  int rare_from = 10;
  int games_played = 0;
  long long max_turns = 0;
  std::uint64_t seed = 1;
  bool progress = false;
  InferParams infer;
  FilterParams filter;
  SimParams sim;
};

int EvalHumanGames(const Options& o, const Setup& setup, std::ostream& records) {
  // (game id, turn) -> rating; empty means every turn.
  std::map<std::pair<std::string, int>, int> wanted;
  if (!o.turns_path.empty()) {
    std::ifstream in(o.turns_path);
    if (!in) {
      std::cerr << "cannot open " << o.turns_path << "\n";
      return 1;
    }
    std::string id;
    int turn = 0, rating = 0;
    for (std::string line; std::getline(in, line);) {
      std::istringstream ss(line);
      if (ss >> id >> turn) wanted[{id, turn}] = (ss >> rating) ? rating : 0;
    }
  }
  std::vector<fs::path> files;
  for (const auto& e : fs::directory_iterator(o.games_dir))
    if (e.path().extension() == ".gcg") files.push_back(e.path());
  std::sort(files.begin(), files.end());

  std::vector<std::unique_ptr<MoveModel>> models;
  for (const std::string& name : o.models) models.push_back(MakeModel(name, setup));
  long long scored = 0;
  for (std::size_t g = 0; g < files.size(); ++g) {
    if (o.max_turns && scored >= o.max_turns) break;
    std::string error;
    auto game = ReadGcgFile(files[g].string(), &error);
    std::vector<ReplayedTurn> turns;
    if (game) turns = ReplayTurns(*game, &error);
    const std::string id = files[g].stem().string();
    for (std::size_t i = 0; i < turns.size(); ++i) {
      const ReplayedTurn& t = turns[i];
      if (!t.known_rack || t.phony || t.move.kind == Move::Kind::kPass) continue;
      if (o.max_turns && scored >= o.max_turns) break;
      int rating = 0;
      if (!wanted.empty()) {
        auto it = wanted.find({id, t.turn});
        if (it == wanted.end()) continue;
        rating = it->second;
      }
      // The observer's rack does not change during the opponent's turn: it is
      // the one they hold on their next turn.
      std::optional<Rack> own;
      for (std::size_t j = i + 1; j < turns.size() && !own; ++j)
        if (turns[j].player != t.player) {
          if (turns[j].known_rack) own = turns[j].rack;
          break;
        }
      if (!own) continue;
      Observation obs;
      obs.board = t.board;
      obs.move = t.move;
      obs.bag_size = t.bag_size;
      obs.own = *own;
      obs.unseen = UnseenTiles(t.board, *own);
      obs.rating = rating;
      if (t.move.kind == Move::Kind::kExchange) continue;  // the observer cannot see the tiles
      const Rack played = MoveTiles(t.move);
      if (obs.RackSize() != t.rack.Size() || !ContainsTiles(obs.unseen, t.rack)) continue;
      const Rack truth = MinusTiles(t.rack, played);
      const Rack possible = MinusTiles(obs.unseen, played);
      for (std::size_t m = 0; m < models.size(); ++m) {
        RackPosterior post = InferRack(obs, *models[m], o.infer).Without(played);
        WriteRecord(records, o.models[m], post, truth, possible,
                    SampledTruthProbability(obs, *models[m], o.infer, truth));
      }
      ++scored;
    }
    if (o.progress) std::cerr << "progress: " << g + 1 << "/" << files.size() << std::endl;
  }
  std::cout << "scored turns: " << scored << "\n";
  return 0;
}

struct Side {
  std::string model_name;
  std::unique_ptr<MoveModel> model;
  std::unique_ptr<RackFilter> filter;  // tracks the other side's rack
  std::unique_ptr<SimBot> bot;
  double rare = 0;
};

int SelfPlay(const Options& o, const Setup& setup, std::ostream& records) {
  MoveGenerator gen(*setup.gaddag);
  std::mt19937_64 rare_rng(MixSeed(o.seed ^ 0x5eed));
  Side sides[2];
  const std::string names[2] = {o.model_a, o.model_b};
  for (int s = 0; s < 2; ++s) {
    sides[s].model_name = names[s];
    sides[s].rare = o.rare[s];
    SimParams sim = o.sim;
    sim.seed = MixSeed(o.seed + s);
    sides[s].bot = std::make_unique<SimBot>(*setup.gaddag, *setup.leaves, sim);
    if (names[s] == "none") continue;
    sides[s].model = MakeModel(names[s], setup);
    FilterParams fp = o.filter;
    fp.seed = MixSeed(o.seed + 10 + s);
    sides[s].filter = std::make_unique<RackFilter>(*sides[s].model, *setup.leaves, fp);
    sides[s].bot->set_rack_sampler(sides[s].filter.get());
  }

  int wins[3] = {0, 0, 0};  // a, b, ties
  long long spread_a = 0, rare_moves = 0, surprises = 0;
  for (int g = 0; g < o.games_played; ++g) {
    Game game(MixSeed(MixSeed(o.seed) + static_cast<std::uint64_t>(g)));
    const int seat_a = g % 2;
    auto side_of = [&](int seat) -> Side& { return sides[seat == seat_a ? 0 : 1]; };
    for (int seat = 0; seat < 2; ++seat)
      if (auto& f = side_of(seat).filter)
        f->Reset(UnseenTiles(game.board(), game.rack(seat)), game.rack(1 - seat).Size());
    while (!game.over()) {
      const int me = game.to_move(), opp = 1 - me;
      Side& mover = side_of(me);
      Side& observer = side_of(opp);
      if (mover.filter)
        WriteRecord(records, std::string(me == seat_a ? "a:" : "b:") + mover.model_name,
                    mover.filter->posterior(), game.rack(opp), mover.filter->unseen());

      Move m;
      if (mover.rare > 0 && std::bernoulli_distribution(mover.rare)(rare_rng)) {
        const Rack unseen = UnseenTiles(game.board(), game.rack(me));
        std::vector<Option> options =
            RankOptions(gen, game.board(), game.rack(me), game.bag_size(), unseen, *setup.leaves);
        if (static_cast<int>(options.size()) > o.rare_from) {
          std::uniform_int_distribution<std::size_t> pick(o.rare_from, options.size() - 1);
          m = options[pick(rare_rng)].move;
          ++rare_moves;
        } else {
          m = mover.bot->Choose(game);
        }
      } else {
        m = mover.bot->Choose(game);
      }

      Observation obs;
      obs.board = game.board();
      obs.move = m;
      obs.bag_size = game.bag_size();
      obs.own = game.rack(opp);
      obs.unseen = UnseenTiles(game.board(), game.rack(opp));
      if (m.kind == Move::Kind::kExchange) {
        obs.exchanged = static_cast<int>(m.tiles.size());
        obs.move.tiles.clear();
      }
      game.Apply(m);
      if (game.over()) break;
      if (observer.filter) {
        observer.filter->ObserveOpponent(obs);
        surprises += observer.filter->surprised();
      }
      if (mover.filter)
        mover.filter->Resync(UnseenTiles(game.board(), game.rack(me)), game.rack(opp).Size());
    }
    const int sa = game.score(seat_a), sb = game.score(1 - seat_a);
    spread_a += sa - sb;
    ++wins[sa > sb ? 0 : sb > sa ? 1 : 2];
    if (o.progress) std::cerr << "progress: " << g + 1 << "/" << o.games_played << std::endl;
  }
  const int n = o.games_played;
  std::cout << "a=" << o.model_a << " vs b=" << o.model_b << " (rare a/b: " << o.rare[0] << "/"
            << o.rare[1] << "): wins a/b/ties: " << wins[0] << "/" << wins[1] << "/" << wins[2]
            << ", a win %: " << (n ? 100.0 * (wins[0] + 0.5 * wins[2]) / n : 0)
            << ", a avg spread: " << (n ? double(spread_a) / n : 0) << ", rare moves: " << rare_moves
            << ", surprises: " << surprises << "\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon, leaves_path, human_path;
  Options o;
  Setup setup;
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
    } else if (a == "--human-params") {
      human_path = value();
    } else if (a == "--tau") {
      setup.tau = std::stod(value());
    } else if (a == "--games") {
      o.games_dir = value();
    } else if (a == "--turns") {
      o.turns_path = value();
    } else if (a == "--models") {
      o.models = SplitList(value());
    } else if (a == "--max-turns") {
      o.max_turns = std::stoll(value());
    } else if (a == "--samples") {
      o.infer.samples = std::stoi(value());
    } else if (a == "--exact-limit") {
      o.infer.exact_limit = o.filter.exact_limit = std::stoll(value());
    } else if (a == "--particles") {
      o.filter.particles = std::stoi(value());
    } else if (a == "--surprise-bits") {
      o.filter.surprise_bits = std::stod(value());
    } else if (a == "--a") {
      o.model_a = value();
    } else if (a == "--b") {
      o.model_b = value();
    } else if (a == "--a-rare") {
      o.rare[0] = std::stod(value());
    } else if (a == "--b-rare") {
      o.rare[1] = std::stod(value());
    } else if (a == "--rare-from") {
      o.rare_from = std::stoi(value());
    } else if (a == "--games-played") {
      o.games_played = std::stoi(value());
    } else if (a == "--sim-candidates") {
      o.sim.candidates = std::stoi(value());
    } else if (a == "--sim-iterations") {
      o.sim.max_iterations = std::stoi(value());
    } else if (a == "--posterior-share") {
      o.sim.posterior_share = std::stod(value());
    } else if (a == "--threads") {
      o.sim.threads = std::stoi(value());
    } else if (a == "--seed") {
      o.seed = std::stoull(value());
    } else if (a == "--records") {
      o.records_path = value();
    } else if (a == "--progress") {
      o.progress = true;
    } else {
      std::cerr << "usage: " << argv[0]
                << " --lexicon FILE --leaves KLV --records JSONL [--human-params FILE] [--tau T]"
                   " (--games DIR [--turns TSV] [--models LIST] [--max-turns N] [--samples N]"
                   " | --games-played N [--a MODEL] [--b MODEL] [--a-rare P] [--b-rare P]"
                   " [--rare-from K] [--sim-candidates N] [--sim-iterations N]"
                   " [--posterior-share P] [--particles N] [--surprise-bits B] [--threads N]"
                   " [--seed S]) [--exact-limit N] [--progress]\n";
      return 2;
    }
  }
  if (lexicon.empty() || leaves_path.empty() || o.records_path.empty() ||
      o.games_dir.empty() == (o.games_played == 0)) {
    std::cerr << "--lexicon, --leaves, --records and one of --games or --games-played are required\n";
    return 2;
  }
  Gaddag gaddag = Gaddag::FromFile(lexicon);
  Leaves leaves = Leaves::FromFile(leaves_path);
  setup.gaddag = &gaddag;
  setup.leaves = &leaves;
  if (!human_path.empty()) setup.human = HumanModelParams::FromFile(human_path);
  std::ofstream records(o.records_path);
  if (!records) {
    std::cerr << "cannot write " << o.records_path << "\n";
    return 1;
  }
  return o.games_dir.empty() ? SelfPlay(o, setup, records) : EvalHumanGames(o, setup, records);
}
