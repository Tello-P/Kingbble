#include "scrabble/inference.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <string>

namespace scrabble {

namespace {

std::string Fixture(const std::string& name) { return std::string(KINGBBLE_FIXTURES) + "/" + name; }

Rack R(const std::string& s) { return *Rack::FromString(s); }

Move Place(int row, int col, Direction dir, const std::string& tiles) {
  Move m;
  m.kind = Move::Kind::kPlace;
  m.row = row;
  m.col = col;
  m.dir = dir;
  m.tiles = tiles;
  return m;
}

class InferenceTest : public ::testing::Test {
 protected:
  Gaddag gaddag_ = Gaddag::FromWords({"AT", "TA", "CAT", "ACT"});
  Leaves leaves_ = Leaves::FromFile(Fixture("leaves_small.klv2"));

  // The opponent opens with AT from a 4-tile rack; the observer cannot see
  // ACCEETTX, two of which are in the opponent's leave.
  Observation OpeningAt() const {
    Observation obs;
    obs.move = Place(7, 6, Direction::kHorizontal, "AT");
    obs.unseen = R("ACCEETTX");
    obs.bag_size = 4;
    return obs;
  }
};

}  // namespace

TEST(RackMath, CountsAndProbabilitiesAgreeWithEnumeration) {
  const Rack pool = R("AABCC?");
  for (int k = 0; k <= 4; ++k) {
    long long n = 0;
    double total = 0;
    ForEachSubRack(pool, k, [&](const Rack& r) {
      ++n;
      EXPECT_EQ(r.Size(), k);
      total += DrawProbability(pool, r);
    });
    EXPECT_EQ(n, CountSubRacks(pool, k, 1000)) << k;
    EXPECT_NEAR(total, 1.0, 1e-12) << k;
  }
  EXPECT_EQ(CountSubRacks(pool, 3, 2), 2);  // capped
  EXPECT_DOUBLE_EQ(DrawProbability(R("AB"), R("A")), 0.5);
  EXPECT_EQ(DrawProbability(R("AB"), R("C")), 0);
  std::mt19937_64 rng(3);
  Rack d = DrawTiles(pool, 4, rng);
  EXPECT_EQ(d.Size(), 4);
  EXPECT_TRUE(ContainsTiles(pool, d));
  EXPECT_EQ(MinusTiles(PlusTiles(R("AB"), R("BC")), R("B")), R("ABC"));
  EXPECT_EQ(CommonTiles(R("AAB?"), R("A?C")), R("A?"));
  EXPECT_EQ(MoveTiles(Place(0, 0, Direction::kHorizontal, "a.T")), R("?T"));
}

TEST_F(InferenceTest, RangeFinderKeepsOnlyRacksWhereTheMoveIsBest) {
  RangeFinderModel model(gaddag_, leaves_);
  Observation obs = OpeningAt();
  RackPosterior post = InferRack(obs, model, {});
  ASSERT_FALSE(post.empty());
  double total = 0;
  for (const Hypothesis& h : post.hypotheses()) {
    total += h.weight;
    EXPECT_EQ(h.rack.Size(), 4);
    EXPECT_TRUE(ContainsTiles(h.rack, R("AT")));
  }
  EXPECT_NEAR(total, 1.0, 1e-12);
  // With a C the opponent would have played CAT.
  EXPECT_EQ(post.TileProbability(*TileFromChar('C')), 0);
  EXPECT_GT(post.TileProbability(*TileFromChar('E')), 0);
  EXPECT_EQ(post.Probability(R("ACTT")), 0);
  EXPECT_GT(post.Probability(R("AEET")), 0);
}

TEST_F(InferenceTest, KeepsThePriorWhenNoRackExplainsTheMove) {
  RangeFinderModel model(gaddag_, leaves_);
  Observation obs = OpeningAt();
  obs.move = Place(7, 6, Direction::kHorizontal, "TX");  // a phony
  RackPosterior post = InferRack(obs, model, {});
  ASSERT_FALSE(post.empty());
  // Prior of a C among two tiles drawn from ACCEET: 1 - C(4,2)/C(6,2).
  EXPECT_NEAR(post.TileProbability(*TileFromChar('C')), 1 - 6.0 / 15, 1e-12);
}

TEST_F(InferenceTest, SampledPosteriorMatchesExactOnTheSameQuestion) {
  RangeFinderModel model(gaddag_, leaves_);
  Observation obs = OpeningAt();
  InferParams sampled;
  sampled.exact_limit = 0;
  sampled.samples = 4000;
  RackPosterior exact = InferRack(obs, model, {});
  RackPosterior approx = InferRack(obs, model, sampled);
  EXPECT_EQ(approx.TileProbability(*TileFromChar('C')), 0);
  EXPECT_NEAR(approx.TileProbability(*TileFromChar('E')), exact.TileProbability(*TileFromChar('E')),
              0.05);
  RackPosterior leaves = exact.Without(R("AT"));
  for (const Hypothesis& h : leaves.hypotheses()) EXPECT_EQ(h.rack.Size(), 2);
}

}  // namespace scrabble

namespace scrabble {

namespace {

std::vector<Option> FakeOptions(int n) {
  std::vector<Option> options(n);
  for (int i = 0; i < n; ++i) {
    options[i].features[kFeatGap] = static_cast<float>(i);
    options[i].features[kFeatLogRank] = static_cast<float>(std::log1p(i));
  }
  return options;
}

}  // namespace

TEST(HumanMoveModel, MixtureSumsToOneAndSharesTheResidual) {
  HumanModelParams params;
  params.top_k = 4;
  params.epsilon = 0.1;
  params.residual = 0.2;
  std::vector<double> p = HumanMoveModel::OptionProbabilities(FakeOptions(10), params, 0);
  ASSERT_EQ(p.size(), 11u);
  double total = 0;
  for (double x : p) total += x;
  EXPECT_NEAR(total, 1.0, 1e-12);
  for (int i = 1; i < 4; ++i) EXPECT_GT(p[i - 1], p[i]);
  // Below the top 4: 6 options and the unmatched slot share 0.9 * 0.2, plus 0.1 / 11 each.
  for (int i = 4; i <= 10; ++i) EXPECT_NEAR(p[i], 0.9 * 0.2 / 7 + 0.1 / 11, 1e-12);
  // Fewer options than top_k: the unmatched slot alone takes the residual.
  std::vector<double> q = HumanMoveModel::OptionProbabilities(FakeOptions(2), params, 0);
  EXPECT_NEAR(q[2], 0.9 * 0.2 + 0.1 / 3, 1e-12);
}

TEST(HumanMoveModel, ReadsBandsFromAFile) {
  const std::string path = ::testing::TempDir() + "/human_model.txt";
  {
    std::ofstream out(path);
    out << "# fitted\ntop_k 32\nepsilon 0.02\nresidual 0.05\n"
        << "band 0 -1 0 0 0 0 0 0 0\nband 1800 -2 0 0 0 0 0 0 0  # experts\n";
  }
  HumanModelParams p = HumanModelParams::FromFile(path);
  EXPECT_EQ(p.top_k, 32);
  EXPECT_DOUBLE_EQ(p.epsilon, 0.02);
  EXPECT_DOUBLE_EQ(p.residual, 0.05);
  EXPECT_EQ(p.WeightsFor(0)[kFeatGap], -1);
  EXPECT_EQ(p.WeightsFor(1799)[kFeatGap], -1);
  EXPECT_EQ(p.WeightsFor(2100)[kFeatGap], -2);
  {
    std::ofstream out(path);
    out << "band 0 -1 0\n";
  }
  EXPECT_THROW(HumanModelParams::FromFile(path), std::runtime_error);
}

TEST_F(InferenceTest, HumanModelMakesRareRacksUnlikelyButPossible) {
  HumanMoveModel model(gaddag_, leaves_, HumanModelParams{});
  Observation obs = OpeningAt();
  RackPosterior post = InferRack(obs, model, {});
  const double c = post.TileProbability(*TileFromChar('C'));
  EXPECT_GT(c, 0);
  EXPECT_LT(c, 1 - 6.0 / 15);  // below the prior: CAT would have been better
  obs.move = Place(7, 6, Direction::kHorizontal, "TX");  // phonies keep some likelihood
  EXPECT_GT(model.Likelihood(obs, R("ACTX")), 0);
}

}  // namespace scrabble
