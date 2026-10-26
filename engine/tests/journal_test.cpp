#include "../apps/journal.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace scrabble {

namespace {

std::string ReadAll(const std::string& path) {
  std::ifstream in(path);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

}  // namespace

TEST(Journal, ResumesAfterTheLastFinishedUnit) {
  const std::string path = ::testing::TempDir() + "/journal_records.jsonl";
  std::filesystem::remove(path);
  std::filesystem::remove(path + ".done");
  {
    Journal j(path);
    ASSERT_TRUE(j.ok());
    EXPECT_TRUE(j.done().empty());
    j.unit() << "a1\na2\n";
    j.Finish("game0", {400, 380});
    j.unit() << "b1\n";  // interrupted before Finish
  }
  {
    std::ofstream(path, std::ios::app) << "torn";  // a partial write
    Journal j(path);
    EXPECT_EQ(ReadAll(path), "a1\na2\n");
    ASSERT_TRUE(j.IsDone("game0"));
    EXPECT_FALSE(j.IsDone("game1"));
    EXPECT_EQ(j.done().at("game0"), (std::vector<double>{400, 380}));
    j.unit() << "c1\n";
    j.Finish("game1", {1});
  }
  Journal j(path);
  EXPECT_EQ(j.done().size(), 2u);
  EXPECT_EQ(ReadAll(path), "a1\na2\nc1\n");
}

TEST(Journal, StartsFreshWithoutADoneFile) {
  const std::string path = ::testing::TempDir() + "/journal_fresh.jsonl";
  std::filesystem::remove(path + ".done");
  std::ofstream(path) << "stale\n";
  Journal j(path);
  EXPECT_TRUE(j.done().empty());
  EXPECT_EQ(ReadAll(path), "");
}

}  // namespace scrabble
