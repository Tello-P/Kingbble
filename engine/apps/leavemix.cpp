// Diagnostic mixes of two leave tables, built where they are used and never
// published: writes "leave,delta" lines that, added to table OURS, give table
// BASE's value for every leave whose size is in --sizes (e.g. "6").
//   kingbble_leavemix --base KLV --ours CSV [--ours-deltas CSV] --sizes 6 --leaves-list CSV --out
//   FILE
// --leaves-list: the leaves to consider, "LEAVE,..." lines (scripts/zero_leaves.py).
#include <fstream>
#include <iostream>
#include <set>
#include <string>

#include "kingbble/leaves.hpp"
#include "kingbble/rack.hpp"

using namespace kingbble;

int main(int argc, char** argv) {
  std::string base_path, ours_path, ours_deltas, list_path, out_path, sizes = "6";
  for (int i = 1; i + 1 < argc; i += 2) {
    const std::string a = argv[i], v = argv[i + 1];
    if (a == "--base")
      base_path = v;
    else if (a == "--ours")
      ours_path = v;
    else if (a == "--ours-deltas")
      ours_deltas = v;
    else if (a == "--sizes")
      sizes = v;
    else if (a == "--leaves-list")
      list_path = v;
    else if (a == "--out")
      out_path = v;
    else {
      std::cerr << "unknown option " << a << "\n";
      return 2;
    }
  }
  if (base_path.empty() || ours_path.empty() || list_path.empty() || out_path.empty()) {
    std::cerr << "--base, --ours, --leaves-list and --out are required\n";
    return 2;
  }
  std::set<int> keep;
  for (char c : sizes)
    if (c >= '1' && c <= '9') keep.insert(c - '0');
  const Leaves base = Leaves::FromFile(base_path);
  Leaves ours = Leaves::FromFile(ours_path);
  if (!ours_deltas.empty()) ours.AddDeltas(ours_deltas);
  std::ifstream in(list_path);
  std::ofstream out(out_path);
  out << "leave,delta\n";
  std::string line;
  std::getline(in, line);
  long n = 0;
  while (std::getline(in, line)) {
    const auto comma = line.find(',');
    const auto leave = Rack::FromString(line.substr(0, comma));
    if (!leave || !keep.count(leave->Size())) continue;
    const double d = base.Value(*leave) - ours.Value(*leave);
    if (d != 0) {
      out << line.substr(0, comma) << "," << d << "\n";
      ++n;
    }
  }
  std::cout << "mixed " << n << " leaves of sizes " << sizes << "\n";
  return 0;
}
