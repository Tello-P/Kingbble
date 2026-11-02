// Self-play: builds the GADDAG, plays N games between two bots and reports
// results and throughput. Bots swap seats every game, so each starts half of them.
//
//   kingbble_selfplay --lexicon lexica/ENABLE.txt --games 100 --seed 1 [--verbose] [--progress]
//   kingbble_selfplay --lexicon lexica/ENABLE.txt --leaves lexica/NWL23.klv2 --a equity --b greedy
//   kingbble_selfplay ... --a sim --b equity --sim-candidates 10 --sim-iterations 200 --threads 8
//   kingbble_selfplay ... --a equity+eg --b equity --endgame-time 5
//   kingbble_selfplay ... --a equity+peg+eg --b equity+eg --peg-bag 1 --peg-time 10
// A "+eg" suffix makes a bot solve the endgame once the bag is empty; "+peg"
// makes it solve the pre-endgame with 1..peg-bag tiles in the bag (the best
// --peg-candidates plays by static equity, within --peg-time seconds).
// "+apeg" plays the approximate pre-endgame with peg-bag + 1..6 tiles in the bag
// (--apeg-candidates plays, --apeg-samples deals, --apeg-plies endgame depth,
// within --apeg-time seconds, on --threads threads).
// With --results FILE every finished game is appended to FILE and a rerun skips
// the games already there; --status prints the totals of FILE and exits.
// Suffixes stack: "sim+peg+eg", "equity+apeg+peg+eg".
//   kingbble_selfplay ... --a risk --b sim --paired --risk-lambda 1 --risk-shrink 25
// "risk" and "riskwin" pick among the simulated candidates by mean - lambda *
// variance or by win % with 7-25 tiles in the bag (RiskParams): --risk-lambda
// scales the theoretical lambda, --risk-rest-sd sets the spread to come per
// root of a tile, --risk-shrink the variance shrinkage, --risk-relative uses
// the variance relative to the best candidate and --risk-iterations multiplies
// the iterations inside the window. --sim-plies sets both bots' playout depth
// and --a-plies bot a's alone. Result lines end with each bot's moves and
// seconds (", a M moves T s, b M moves T s"). --paired plays each deal twice, games 2k-1 and 2k,
// with the bots swapping seats, so luck of the draw cancels out in each pair.
// --risk-policy FILE sets lambda_scale per bag and lead bucket (a policy learned
// by ml.rl.risk_policy, "bag_lo bag_hi lead_lo lead_hi scale" lines).
// --gcg-dir DIR saves game N as DIR/gameN.gcg with both racks (training data).
// --a-leaves FILE gives bot a its own leave table (KLV2 or CSV), e.g. a fitted
// one against the published table with --paired.
// "human" plays moves drawn from the human move model (--human-params, with
// --word-freq and --alt-lexicon as it was fitted, for a player of --human-rating).
// "sim+inf:MODEL" tracks the opponent's rack with MODEL (human, rf, none) and
// draws its simulated opponent racks from it (--posterior-share of them; the
// filter takes --particles, --exact-limit and --surprise-bits, RangeFinder
// --tau), e.g. --a sim+inf:human+eg --b sim+eg --paired.
// Reproducible runs: --per-game-seeds restarts every bot's random streams at
// each game from the seed, the deal and the seat, so a game does not depend on
// the games before it (a resumed run plays the same games) and both games of a
// pair share the streams of each seat. Give fixed iterations (no --sim-time)
// and, for "+eg", --endgame-time 0 with --endgame-plies N and
// --endgame-threads 1. With --results the run's manifest (--tag KEY=VALUE
// entries, the config with file hashes, the command line) is written to
// <results>.manifest.json; resuming with another config fails.

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include "bot_factory.hpp"
#include "game_record.hpp"
#include "manifest.hpp"
#include "results_file.hpp"
#include "run_seeds.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/game.hpp"

using namespace scrabble;

namespace {

// Totals over every finished game ("game N: S1 - S2, a in seat K" lines).
void PrintSummary(const ResultsFile& results, int games, const std::string& kind_a,
                  const std::string& kind_b) {
  long long total_score = 0, spread_a = 0;
  int wins[3] = {0, 0, 0};     // by seat: p1, p2, ties
  int wins_ab[3] = {0, 0, 0};  // by bot: a, b, ties
  int n = 0;
  for (const std::string& line : results.lines()) {
    int g = 0, s0 = 0, s1 = 0, seat = 0;
    if (std::sscanf(line.c_str(), "game %d: %d - %d, a in seat %d", &g, &s0, &s1, &seat) != 4 ||
        g > games)
      continue;
    ++n;
    total_score += s0 + s1;
    ++wins[s0 > s1 ? 0 : s1 > s0 ? 1 : 2];
    const int sa = seat == 1 ? s0 : s1, sb = seat == 1 ? s1 : s0;
    spread_a += sa - sb;
    ++wins_ab[sa > sb ? 0 : sb > sa ? 1 : 2];
  }
  std::cout << "games: " << n << " of " << games << ", wins p1/p2/ties: " << wins[0] << "/"
            << wins[1] << "/" << wins[2]
            << ", avg score per player: " << (n ? total_score / (2.0 * n) : 0) << "\n";
  std::cout << "a=" << kind_a << " vs b=" << kind_b << ": wins a/b/ties: " << wins_ab[0] << "/"
            << wins_ab[1] << "/" << wins_ab[2]
            << ", a win %: " << (n ? 100.0 * (wins_ab[0] + 0.5 * wins_ab[2]) / n : 0)
            << ", a avg spread: " << (n ? double(spread_a) / n : 0) << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon, leaves_path, a_leaves_path, kind_a = "greedy", kind_b = "greedy";
  int games = 10;
  std::uint64_t seed = 1;
  bool verbose = false;
  bool progress = false;  // "progress: G/N" on stderr after each game
  std::string results_path, gcg_dir, human_path, freq_path, alt_path;
  int human_rating = 0;
  bool status_only = false;
  bool paired = false;
  bool endgame_fallback = false;  // "+eg" bots play the inner move when not solved exactly
  bool endgame_stats = false;     // per "+eg" bot: solves, exact ones, moves changed vs inner
  bool per_game_seeds = false;    // restart the bots' streams at each game (reproducible)
  Manifest manifest;
  InferenceOptions inference;
  int a_plies = 0;  // 0: as b
  SimParams sim;
  RiskParams risk;
  double endgame_time = 5;
  PegOptions peg;
  peg.params.candidates = 10;
  peg.params.time_limit_s = 10;
  peg.approx.time_limit_s = 10;
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
    } else if (a == "--a-leaves") {
      a_leaves_path = value();
    } else if (a == "--a") {
      kind_a = value();
    } else if (a == "--b") {
      kind_b = value();
    } else if (a == "--sim-candidates") {
      sim.candidates = std::stoi(value());
    } else if (a == "--sim-iterations") {
      sim.max_iterations = std::stoi(value());
    } else if (a == "--a-plies") {
      a_plies = std::stoi(value());
    } else if (a == "--sim-plies") {
      sim.plies = std::stoi(value());
    } else if (a == "--sim-time") {
      sim.time_limit_s = std::stod(value());
    } else if (a == "--endgame-time") {
      endgame_time = std::stod(value());
    } else if (a == "--peg-bag") {
      peg.max_bag = std::stoi(value());
    } else if (a == "--peg-candidates") {
      peg.params.candidates = std::stoi(value());
    } else if (a == "--peg-time") {
      peg.params.time_limit_s = std::stod(value());
    } else if (a == "--apeg-candidates") {
      peg.approx.candidates = std::stoi(value());
    } else if (a == "--apeg-samples") {
      peg.approx.samples = std::stoi(value());
    } else if (a == "--apeg-plies") {
      peg.approx.endgame_plies = std::stoi(value());
    } else if (a == "--apeg-time") {
      peg.approx.time_limit_s = std::stod(value());
    } else if (a == "--threads") {
      sim.threads = std::stoi(value());
    } else if (a == "--games") {
      games = std::stoi(value());
    } else if (a == "--seed") {
      seed = std::stoull(value());
    } else if (a == "--verbose") {
      verbose = true;
    } else if (a == "--progress") {
      progress = true;
    } else if (a == "--results") {
      results_path = value();
    } else if (a == "--gcg-dir") {
      gcg_dir = value();
    } else if (a == "--human-params") {
      human_path = value();
    } else if (a == "--human-rating") {
      human_rating = std::stoi(value());
    } else if (a == "--word-freq") {
      freq_path = value();
    } else if (a == "--alt-lexicon") {
      alt_path = value();
    } else if (a == "--status") {
      status_only = true;
    } else if (a == "--endgame-stats") {
      endgame_stats = true;
    } else if (a == "--endgame-fallback") {
      endgame_fallback = true;
    } else if (a == "--paired") {
      paired = true;
    } else if (a == "--per-game-seeds") {
      per_game_seeds = true;
    } else if (a == "--endgame-plies") {
      peg.endgame_plies = std::stoi(value());
    } else if (a == "--endgame-threads") {
      peg.endgame_threads = std::stoi(value());
    } else if (a == "--posterior-share") {
      sim.posterior_share = std::stod(value());
    } else if (a == "--particles") {
      inference.filter.particles = std::stoi(value());
    } else if (a == "--exact-limit") {
      inference.filter.exact_limit = std::stoll(value());
    } else if (a == "--surprise-bits") {
      inference.filter.surprise_bits = std::stod(value());
    } else if (a == "--tau") {
      inference.tau = std::stod(value());
    } else if (a == "--tag") {
      const std::string tag = value();
      const std::size_t eq = tag.find('=');
      if (eq == std::string::npos) {
        std::cerr << "--tag wants KEY=VALUE\n";
        return 2;
      }
      manifest.Tag(tag.substr(0, eq), tag.substr(eq + 1));
    } else if (a == "--risk-policy") {
      risk.policy = LoadRiskPolicy(value());
    } else if (a == "--risk-lambda") {
      risk.lambda_scale = std::stod(value());
    } else if (a == "--risk-shrink") {
      risk.shrink = std::stod(value());
    } else if (a == "--risk-relative") {
      risk.relative_variance = true;
    } else if (a == "--risk-iterations") {
      risk.iterations_factor = std::stoi(value());
    } else if (a == "--risk-rest-sd") {
      risk.win.rest_sd_per_sqrt_tile = std::stod(value());
    } else {
      std::cerr << "usage: " << argv[0]
                << " --lexicon FILE [--leaves KLV] [--a-leaves KLV] [--a BOT] [--b BOT] [--games "
                   "N] [--seed S]"
                   " [--sim-candidates N] [--sim-iterations N] [--sim-plies N] [--a-plies N] "
                   "[--sim-time S]"
                   " [--threads N] [--endgame-time S]"
                   " [--peg-bag N] [--peg-candidates N] [--peg-time S] [--apeg-candidates N]"
                   " [--apeg-samples N] [--apeg-plies N] [--apeg-time S] [--verbose]"
                   " [--progress] [--results FILE [--status]] [--paired] [--risk-lambda L] "
                   "[--risk-policy FILE]"
                   " [--risk-rest-sd S] [--risk-shrink K] [--risk-relative]"
                   " [--risk-iterations F] [--endgame-stats] [--endgame-fallback] [--gcg-dir DIR] "
                   "[--human-params FILE]"
                   " [--human-rating R] [--word-freq TSV] [--alt-lexicon FILE]"
                   " [--per-game-seeds] [--endgame-plies N] [--endgame-threads N]"
                   " [--posterior-share P] [--particles N] [--exact-limit N]"
                   " [--surprise-bits B] [--tau T] [--tag KEY=VALUE]\n";
      return 2;
    }
  }
  if (status_only) {
    PrintSummary(ResultsFile(results_path, true), games, kind_a, kind_b);
    return 0;
  }
  if (lexicon.empty()) {
    std::cerr << "--lexicon is required\n";
    return 2;
  }

  using Clock = std::chrono::steady_clock;
  auto t0 = Clock::now();
  Gaddag gaddag = Gaddag::FromFile(lexicon);
  double build_s = std::chrono::duration<double>(Clock::now() - t0).count();
  std::cout << "lexicon: " << gaddag.NumWords() << " words, " << gaddag.NumNodes() << " nodes, "
            << gaddag.NumEdges() << " edges, built in " << build_s << " s\n";

  std::unique_ptr<Leaves> leaves, a_leaves;
  if (!a_leaves_path.empty()) a_leaves = std::make_unique<Leaves>(Leaves::FromFile(a_leaves_path));
  if (!leaves_path.empty()) {
    leaves = std::make_unique<Leaves>(Leaves::FromFile(leaves_path));
    std::cout << "leaves: " << leaves->Size() << "\n";
  }
  sim.seed = seed;
  SimParams a_sim = sim;
  if (a_plies > 0) a_sim.plies = a_plies;
  HumanModelParams human_params;
  std::optional<FeatureLexicon> words;  // the human model's word features
  HumanBotOptions human;
  human.rating = human_rating;
  if (!human_path.empty()) {
    human_params = HumanModelParams::FromFile(human_path);
    if (human_params.UsesExtendedFeatures())
      words = FeatureLexicon::Load(lexicon, freq_path, alt_path);
    human.params = &human_params;
    human.lexicon = words ? &*words : nullptr;
  }
  BotSetup setups[2] = {
      MakeBotSetup(kind_a, gaddag, a_leaves ? a_leaves.get() : leaves.get(), a_sim, endgame_time,
                   peg, risk, human, inference),
      MakeBotSetup(kind_b, gaddag, leaves.get(), sim, endgame_time, peg, risk, human, inference)};

  if (!results_path.empty()) {
    // Everything the games depend on; --games may grow on a rerun.
    manifest.SetArgv(argc, argv);
    manifest.Set("tool", "kingbble_selfplay");
    manifest.Set("a", kind_a);
    manifest.Set("b", kind_b);
    manifest.Set("seed", std::to_string(seed));
    manifest.Set("paired", paired ? "1" : "0");
    manifest.Set("per_game_seeds", per_game_seeds ? "1" : "0");
    manifest.Set("lexicon", std::filesystem::path(lexicon).filename().string());
    manifest.Set("lexicon_hash", FileHash(lexicon));
    manifest.Set("leaves_hash", leaves_path.empty() ? "none" : FileHash(leaves_path));
    manifest.Set("a_leaves_hash", a_leaves_path.empty() ? "none" : FileHash(a_leaves_path));
    manifest.Set("human_params_hash", human_path.empty() ? "none" : FileHash(human_path));
    manifest.Set("word_freq_hash", freq_path.empty() ? "none" : FileHash(freq_path));
    manifest.Set("alt_lexicon_hash", alt_path.empty() ? "none" : FileHash(alt_path));
    manifest.Set("human_rating", human_rating);
    manifest.Set("threads", sim.threads);
    manifest.Set("sim_candidates", sim.candidates);
    manifest.Set("sim_iterations", sim.max_iterations);
    manifest.Set("sim_plies", sim.plies);
    manifest.Set("a_plies", a_sim.plies);
    manifest.Set("sim_time", sim.time_limit_s);
    manifest.Set("posterior_share", sim.posterior_share);
    manifest.Set("particles", inference.filter.particles);
    manifest.Set("exact_limit", static_cast<double>(inference.filter.exact_limit));
    manifest.Set("surprise_bits", inference.filter.surprise_bits);
    manifest.Set("tau", inference.tau);
    manifest.Set("endgame_time", endgame_time);
    manifest.Set("endgame_plies", peg.endgame_plies);
    manifest.Set("endgame_threads", peg.endgame_threads);
    manifest.Set("endgame_fallback", endgame_fallback ? "1" : "0");
    manifest.Set("peg", std::to_string(peg.max_bag) + "/" + std::to_string(peg.params.candidates) +
                            "/" + std::to_string(peg.params.time_limit_s));
    manifest.Set("apeg", std::to_string(peg.approx.candidates) + "/" +
                             std::to_string(peg.approx.samples) + "/" +
                             std::to_string(peg.approx.endgame_plies) + "/" +
                             std::to_string(peg.approx.time_limit_s));
    manifest.Set("risk", std::to_string(risk.lambda_scale) + "/" + std::to_string(risk.shrink) +
                             "/" + std::to_string(risk.relative_variance) + "/" +
                             std::to_string(risk.iterations_factor) + "/" +
                             std::to_string(risk.win.rest_sd_per_sqrt_tile) +
                             (risk.policy.empty() ? "" : "/policy"));
    // Timed searches make the games depend on the machine's speed.
    const bool timed = sim.time_limit_s > 0 ||
                       ((kind_a + kind_b).find("+eg") != std::string::npos &&
                        (endgame_time > 0 || peg.endgame_threads != 1)) ||
                       (kind_a + kind_b).find("peg") != std::string::npos;
    manifest.Set("reproducible", per_game_seeds && !timed ? "1" : "0");
    const std::string manifest_path = results_path + ".manifest.json";
    const std::string old = Manifest::ReadConfig(manifest_path);
    const ResultsFile existing(results_path, true);
    if (!old.empty() && !existing.lines().empty() && old != manifest.ConfigJson()) {
      std::cerr << results_path << " holds games played with another config (see " << manifest_path
                << "); start a new results file or delete it\n";
      return 2;
    }
    if (!manifest.Write(manifest_path)) {
      std::cerr << "cannot write " << manifest_path << "\n";
      return 1;
    }
  }
  if (endgame_stats)
    for (auto& s : setups)
      if (auto* eg = dynamic_cast<EndgameBot*>(s.bot.get())) eg->set_compare_inner(true);
  if (endgame_fallback)
    for (auto& s : setups)
      if (auto* eg = dynamic_cast<EndgameBot*>(s.bot.get())) eg->set_inexact_fallback(true);
  if (!gcg_dir.empty()) std::filesystem::create_directories(gcg_dir);
  const std::string lexicon_name = std::filesystem::path(lexicon).stem().string();

  long long turns = 0, generated = 0;
  double gen_s = 0;
  ResultsFile results(results_path);
  auto report = [&] {
    int done = 0;
    for (int g = 1; g <= games; ++g) done += results.Done(g);
    if (progress) std::cerr << "progress: " << done << "/" << games << std::endl;
  };
  report();
  for (int g = 0; g < games; ++g) {
    if (results.Done(g + 1)) continue;
    const int deal = paired ? g / 2 : g;
    Game game(DealSeed(seed, deal));
    const int seat_a = g % 2;  // seat of bot a in this game
    int moves[2] = {0, 0};     // by bot: a, b
    double seconds[2] = {0, 0};
    const std::array<std::string, 2> seat_kinds = seat_a == 0
                                                      ? std::array<std::string, 2>{kind_a, kind_b}
                                                      : std::array<std::string, 2>{kind_b, kind_a};
    GameRecord record({"p1", "p2"}, seat_kinds, lexicon_name,
                      "selfplay seed " + std::to_string(seed) + " game " + std::to_string(g + 1));
    for (int side = 0; side < 2; ++side) {
      const int seat = side == 0 ? seat_a : 1 - seat_a;
      std::optional<std::uint64_t> stream;
      if (per_game_seeds) stream = StreamSeed(seed, deal, seat);
      setups[side].NewGame(game, seat, stream);
    }
    while (!game.over()) {
      const int side = game.to_move() == seat_a ? 0 : 1;
      Bot& bot = *setups[side].bot;
      auto t = Clock::now();
      Move m = bot.Choose(game);
      const double spent = std::chrono::duration<double>(Clock::now() - t).count();
      gen_s += spent;
      seconds[side] += spent;
      ++moves[side];
      generated += static_cast<long long>(bot.last_num_moves());
      ++turns;
      if (verbose) {
        std::cout << "  p" << game.to_move() + 1 << " " << game.rack(game.to_move()).ToString()
                  << "  " << m.ToString() << "\n";
      }
      record.Add(game, m);
      for (BotSetup& s : setups) s.BeforeMove(game, m);
      game.Apply(m);
      for (BotSetup& s : setups) s.AfterMove(game);
    }
    record.Finish(game);
    if (!gcg_dir.empty())
      std::ofstream(gcg_dir + "/game" + std::to_string(g + 1) + ".gcg") << WriteGcg(record.gcg());
    const int s0 = game.score(0), s1 = game.score(1);
    char timing[96];
    std::snprintf(timing, sizeof timing, ", a %d moves %.2f s, b %d moves %.2f s", moves[0],
                  seconds[0], moves[1], seconds[1]);
    results.Append(g + 1, "game " + std::to_string(g + 1) + ": " + std::to_string(s0) + " - " +
                              std::to_string(s1) + ", a in seat " + std::to_string(seat_a + 1) +
                              timing);
    if (verbose) std::cout << "game " << g + 1 << ": " << s0 << " - " << s1 << "\n";
    report();
  }
  PrintSummary(results, games, kind_a, kind_b);
  for (int side = 0; side < 2 && endgame_stats; ++side)
    if (auto* eg = dynamic_cast<EndgameBot*>(setups[side].bot.get())) {
      const EndgameBot::Stats& st = eg->stats();
      std::cout << (side == 0 ? "a" : "b") << " endgames: " << st.solves << " solved, " << st.exact
                << " exact, avg depth " << (st.solves ? double(st.depth_sum) / st.solves : 0)
                << ", move differs from the inner bot in " << st.changed << " of " << st.compared
                << "\n";
    }
  std::cout << "turns: " << turns
            << ", avg moves per turn: " << (turns ? double(generated) / turns : 0)
            << ", positions/s: " << (gen_s > 0 ? turns / gen_s : 0)
            << ", moves/s: " << (gen_s > 0 ? generated / gen_s : 0) << "\n";
  return 0;
}
