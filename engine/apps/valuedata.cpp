// Training data for the value model: plays static-equity self-play games and, at
// sampled positions with tiles in the bag, writes the position's features (from
// the side of the player to move, ValueFeatures) with two labels: the share of
// --rollouts playouts to the end that the player to move won (the opponent's rack
// and the bag dealt anew each time, ties half), and who won the game itself.
//
//   kingbble_valuedata --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2
//       --games 1000 --seed 1 --out value.csv [--sample 0.15] [--rollouts 16]
//       [--sim-time S [--player-model MODEL]]
// With --sim-time the games themselves are played by the simulating bot, S seconds
// per move (as the matches, with end_unseen 14 and late_bag 7), so the positions and
// the game results come from stronger play; the rollouts stay static equity. With
// --player-model that bot also uses a value model, for rounds of self-improvement.
//   kingbble_valuedata --leaves KLV --from-gcg DIR --out value.csv   (rows from the games
//       saved under DIR, both players' positions with tiles in the bag, both labels set
//       to who won the game)
//   kingbble_valuedata --predict MODEL --in ROWS   (one chance per line: checks a model
//       written by scripts/train_value.py against LightGBM's own predictions)

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "scrabble/equity.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/game.hpp"
#include "scrabble/gcg.hpp"
#include "scrabble/sim_bot.hpp"
#include "scrabble/value_model.hpp"

using namespace scrabble;

namespace {

std::uint64_t MixSeed(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

// The position with the opponent's rack and the bag dealt anew from the unseen tiles.
Game Deal(const Game& game, std::uint64_t seed) {
  const int me = game.to_move();
  const Rack unseen = UnseenTiles(game.board(), game.rack(me));
  const int opp_size = unseen.Size() - game.bag_size();
  std::vector<Tile> pool;
  for (int t = 0; t < kNumTileTypes; ++t) pool.insert(pool.end(), unseen.Count(Tile(t)), Tile(t));
  std::mt19937_64 rng(seed);
  std::shuffle(pool.begin(), pool.end(), rng);
  std::array<Rack, 2> racks;
  racks[me] = game.rack(me);
  Rack bag;
  for (std::size_t i = 0; i < pool.size(); ++i)
    (i < static_cast<std::size_t>(opp_size) ? racks[1 - me] : bag).Add(pool[i]);
  return Game::FromPosition(game.board(), racks, {game.score(0), game.score(1)}, me, bag,
                            game.scoreless_turns(), seed);
}

// Rows from a saved game: each turn with tiles in the bag, labelled by who won.
void RowsFromGcg(const GcgGame& gcg, const Leaves& leaves, std::ostream& out) {
  Board board;
  std::array<int, 2> score{0, 0};
  int scoreless = 0;
  std::vector<std::pair<std::array<double, kValueFeatures>, int>> rows;
  for (const GcgEvent& e : gcg.events) {
    const bool turn = e.kind == GcgEvent::Kind::kPlace || e.kind == GcgEvent::Kind::kPass ||
                      e.kind == GcgEvent::Kind::kExchange;
    if (!turn) continue;
    Rack own;
    for (char ch : e.rack) own.Add(ch == '?' ? kBlank : *TileFromChar(ch));
    const Rack unseen = UnseenTiles(board, own);
    const int bag = unseen.Size() - kRackSize;
    if (own.Size() == kRackSize && bag > 0) {
      std::array<Rack, 2> racks;
      racks[e.player] = own;
      Rack in_bag;
      int i = 0;
      for (int t = 0; t < kNumTileTypes; ++t)
        for (int n = 0; n < unseen.Count(Tile(t)); ++n, ++i)
          (i < bag ? in_bag : racks[1 - e.player]).Add(Tile(t));
      const Game g =
          Game::FromPosition(board, racks, {score[0], score[1]}, e.player, in_bag, scoreless, 1);
      rows.emplace_back(ValueFeatures(g, leaves), e.player);
    }
    if (e.kind == GcgEvent::Kind::kPlace) {
      const Move& m = e.move;
      const int dr = m.dir == Direction::kVertical ? 1 : 0;
      for (std::size_t k = 0; k < m.tiles.size(); ++k) {
        const int r = m.row + dr * static_cast<int>(k);
        const int c = m.col + (1 - dr) * static_cast<int>(k);
        const char ch = m.tiles[k];
        if (ch == '.' || !board.IsEmpty(r, c)) continue;
        board.Place(r, c, *TileFromChar(std::toupper(static_cast<unsigned char>(ch))),
                    std::islower(static_cast<unsigned char>(ch)));
      }
      score[e.player] += e.score;
      scoreless = e.score ? 0 : scoreless + 1;
    } else {
      ++scoreless;
    }
  }
  // The final totals, end-of-game rack points included.
  std::array<int, 2> total{0, 0};
  for (const GcgEvent& e : gcg.events) total[e.player] = e.cumulative;
  for (const auto& [features, player] : rows) {
    const int spread = total[player] - total[1 - player];
    const double won = spread > 0 ? 1 : spread == 0 ? 0.5 : 0;
    for (double x : features) out << x << ",";
    out << won << "," << won << "\n";
  }
}

double Points(const Game& over, int me) {
  const int spread = over.score(me) - over.score(1 - me);
  return spread > 0 ? 1 : spread == 0 ? 0.5 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon, leaves_path, out_path, predict_path, in_path, gcg_dir, player_model;
  int games = 100, rollouts = 16;
  double sample = 0.15, sim_time = 0;
  std::uint64_t seed = 1;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
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
    } else if (a == "--out") {
      out_path = value();
    } else if (a == "--games") {
      games = std::stoi(value());
    } else if (a == "--seed") {
      seed = std::stoull(value());
    } else if (a == "--sample") {
      sample = std::stod(value());
    } else if (a == "--rollouts") {
      rollouts = std::stoi(value());
    } else if (a == "--sim-time") {
      sim_time = std::stod(value());
    } else if (a == "--player-model") {
      player_model = value();
    } else if (a == "--from-gcg") {
      gcg_dir = value();
    } else if (a == "--predict") {
      predict_path = value();
    } else if (a == "--in") {
      in_path = value();
    } else {
      std::cerr << "usage: " << argv[0] << " --lexicon FILE --leaves KLV --out FILE [--games N]"
                << " [--seed S] [--sample P] [--rollouts N]\n";
      return 2;
    }
  }
  if (!predict_path.empty()) {
    const ValueModel model = ValueModel::FromFile(predict_path);
    std::ifstream in(in_path);
    std::cout.precision(17);
    for (std::string line; std::getline(in, line);) {
      std::array<double, kValueFeatures> f{};
      std::stringstream cells(line);
      std::string cell;
      for (int i = 0; i < kValueFeatures && std::getline(cells, cell, ','); ++i)
        f[i] = std::stod(cell);
      std::cout << model.Predict(f) << "\n";
    }
    return 0;
  }
  if (!gcg_dir.empty()) {
    const Leaves leaves = Leaves::FromFile(leaves_path);
    std::ofstream out(out_path, std::ios::app);
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(gcg_dir))
      if (entry.path().extension() == ".gcg") files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    for (const auto& file : files)
      if (const auto gcg = ReadGcgFile(file.string())) RowsFromGcg(*gcg, leaves, out);
    std::cerr << files.size() << " games\n";
    return 0;
  }
  if (lexicon.empty() || leaves_path.empty() || out_path.empty()) {
    std::cerr << "--lexicon, --leaves and --out are required\n";
    return 2;
  }
  Gaddag gaddag = Gaddag::FromFile(lexicon);
  Leaves leaves = Leaves::FromFile(leaves_path);
  EquityBot bot(gaddag, leaves);
  SimParams sim;
  sim.candidates = 8;
  sim.max_iterations = 100000;
  sim.time_limit_s = sim_time;
  sim.end_unseen = 14;
  sim.late_bag = 7;
  sim.seed = seed;
  ValueModel model;
  if (!player_model.empty()) {
    model = ValueModel::FromFile(player_model);
    sim.value_model = &model;
  }
  SimBot player(gaddag, leaves, sim);
  std::ofstream out(out_path, std::ios::app);
  std::mt19937_64 pick(MixSeed(seed ^ 0xD1CE));
  std::uniform_real_distribution<double> unit(0, 1);
  for (int g = 0; g < games; ++g) {
    Game game(MixSeed(MixSeed(seed) + static_cast<std::uint64_t>(g)));
    struct Row {
      std::array<double, kValueFeatures> features;
      double rollout;
      int mover;
    };
    std::vector<Row> rows;
    while (!game.over()) {
      if (game.bag_size() > 0 && unit(pick) < sample) {
        const int me = game.to_move();
        double won = 0;
        for (int r = 0; r < rollouts; ++r) {
          Game play = Deal(game, MixSeed(seed * 1000003 + g * 7919 + r * 31 + rows.size()));
          while (!play.over()) play.Apply(bot.Choose(play));
          won += Points(play, me);
        }
        rows.push_back({ValueFeatures(game, leaves), won / rollouts, me});
      }
      game.Apply(sim_time > 0 && game.bag_size() > 0 ? player.Choose(game) : bot.Choose(game));
    }
    for (const Row& row : rows) {
      for (double x : row.features) out << x << ",";
      out << row.rollout << "," << Points(game, row.mover) << "\n";
    }
    out.flush();
  }
  return 0;
}
