#include "scrabble/inference.hpp"

#include <gtest/gtest.h>

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
  // Prior of a C among two tiles drawn from CCEETX: 1 - C(4,2)/C(6,2).
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
