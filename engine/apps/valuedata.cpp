// Training data for the value model: plays static-equity self-play games and, at
// sampled positions with tiles in the bag, writes the position's features (from
// the side of the player to move, ValueFeatures) with two labels: the share of
// --rollouts playouts to the end that the player to move won (the opponent's rack
// and the bag dealt anew each time, ties half), and who won the game itself.
//
//   kingbble_valuedata --lexicon lexica/CSW24.txt --leaves lexica/CSW24.klv2
//       --games 1000 --seed 1 --out value.csv [--sample 0.15] [--rollouts 16]
//   kingbble_valuedata --predict MODEL --in ROWS   (one chance per line: checks a model
//       written by scripts/train_value.py against LightGBM's own predictions)

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "scrabble/equity.hpp"
#include "scrabble/equity_bot.hpp"
#include "scrabble/game.hpp"
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

double Points(const Game& over, int me) {
  const int spread = over.score(me) - over.score(1 - me);
  return spread > 0 ? 1 : spread == 0 ? 0.5 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::string lexicon, leaves_path, out_path, predict_path, in_path;
  int games = 100, rollouts = 16;
  double sample = 0.15;
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
  if (lexicon.empty() || leaves_path.empty() || out_path.empty()) {
    std::cerr << "--lexicon, --leaves and --out are required\n";
    return 2;
  }
  Gaddag gaddag = Gaddag::FromFile(lexicon);
  Leaves leaves = Leaves::FromFile(leaves_path);
  EquityBot bot(gaddag, leaves);
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
      game.Apply(bot.Choose(game));
    }
    for (const Row& row : rows) {
      for (double x : row.features) out << x << ",";
      out << row.rollout << "," << Points(game, row.mover) << "\n";
    }
    out.flush();
  }
  return 0;
}
