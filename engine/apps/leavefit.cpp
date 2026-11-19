// Leave corrections from kingbble_leavegen sums (Phase 2, run off GitHub):
// the value of each leave is the mean value of the full racks it can be
// completed to, weighted by their draw chances (the wolges and Magpie method),
// less that of a rack drawn from a full bag. Its difference from the published
// value is then shrunk towards zero by how noisy it is (empirical Bayes: the
// true spread of the differences is the covariance of two independent halves
// of the data), capped per generation, and written as "leave,delta" lines for
// --leave-deltas. Only corrections are written, never a leave table.
//
//   kingbble_leavefit --leaves LEAVES [--leave-deltas CSV] --half-a FILE... --half-b FILE...
//       [--target a|b|c] [--cap 3] [--min-delta 0.05] --out DELTAS.csv
// --leave-deltas: the corrections of earlier generations, which the new ones add to
// (the prior is the published table plus them). Target b (win chance) is turned
// into points by the slope that best maps it onto the prior on well-measured leaves.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "leavegen_io.hpp"
#include "scrabble/leaves.hpp"
#include "scrabble/rack.hpp"
#include "scrabble/tiles.hpp"

using namespace scrabble;
using namespace leavegen;

namespace {

struct Acc {
  double w = 0, s = 0, q = 0;  // sum of weights, of weight x rack value, of weight^2 / rack count
};

double LogChoose(int n, int k) {
  if (k < 0 || k > n) return -1e300;
  return std::lgamma(n + 1.0) - std::lgamma(k + 1.0) - std::lgamma(n - k + 1.0);
}

struct Fit {
  std::unordered_map<std::uint64_t, Acc> acc;  // by leave key
  double empty_mean = 0;                       // a rack drawn from a full bag
  double noise = 0;                            // variance of one sample
  long samples = 0, racks = 0;
};

// Accumulates every leave's weighted rack values from one half of the data.
Fit Accumulate(const std::vector<std::string>& files, char target) {
  std::unordered_map<std::uint64_t, Agg> racks;
  Totals totals;
  for (const std::string& f : files) Read(f, racks, totals);
  Fit fit;
  std::array<double, kBuckets> mean{};
  double ss = 0, n_all = 0;
  for (int b = 0; b < kBuckets; ++b) {
    const double n = static_cast<double>(totals.n[b]);
    if (n == 0) continue;
    const double sum = target == 'a' ? totals.a[b] : target == 'b' ? totals.b[b] : totals.c[b];
    const double sq = target == 'a' ? totals.aa[b] : target == 'b' ? totals.bb[b] : totals.cc[b];
    mean[b] = sum / n;
    ss += sq - sum * sum / n;
    n_all += n;
  }
  fit.noise = n_all > 1 ? ss / (n_all - 1) : 0;
  fit.samples = static_cast<long>(n_all);
  const auto& tiles = EnglishTiles();
  int total_tiles = 0;
  for (const TileInfo& t : tiles) total_tiles += t.count;
  double e_w = 0, e_s = 0;
  fit.acc.reserve(1200000);
  for (const auto& [key, agg] : racks) {
    if (agg.n == 0) continue;
    ++fit.racks;
    double centred = target == 'a' ? agg.a : target == 'b' ? agg.b : agg.c;
    for (int b = 0; b < kBuckets; ++b) centred -= agg.nb[b] * mean[b];
    const double value = centred / agg.n;
    // The rack's tile counts.
    std::array<int, kNumTileTypes> c{};
    for (std::uint64_t k = key; k; k >>= 5) ++c[(k & 31) - 1];
    std::vector<int> types;
    for (int t = 0; t < kNumTileTypes; ++t)
      if (c[t]) types.push_back(t);
    // Every sub-multiset L of the rack (size 0..6) and the chance of drawing
    // the rest of the rack to it from the bag less L.
    std::array<int, kNumTileTypes> l{};
    auto visit = [&](auto& self, std::size_t i, int size) -> void {
      if (i == types.size()) {
        if (size == kRackSize) return;
        double logw = -LogChoose(total_tiles - size, kRackSize - size);
        for (int t : types) logw += LogChoose(tiles[t].count - l[t], c[t] - l[t]);
        const double w = std::exp(logw);
        if (size == 0) {
          e_w += w;
          e_s += w * value;
          return;
        }
        Rack leave;
        for (int t : types)
          for (int k = 0; k < l[t]; ++k) leave.Add(Tile(t));
        Acc& a = fit.acc[Leaves::Key(leave)];
        a.w += w;
        a.s += w * value;
        a.q += w * w / agg.n;
        return;
      }
      const int t = types[i];
      for (int k = 0; k <= c[t]; ++k) {
        l[t] = k;
        self(self, i + 1, size + k);
      }
      l[t] = 0;
    };
    visit(visit, 0, 0);
  }
  fit.empty_mean = e_w > 0 ? e_s / e_w : 0;
  return fit;
}

}  // namespace

int main(int argc, char** argv) {
  std::string leaves_path, deltas_path, out_path;
  std::vector<std::string> half_a, half_b;
  char target = 'a';
  double cap = 3, min_delta = 0.05;
  bool smooth6 = false;  // --smooth6: pull 6-tile leaves towards their 5-tile sub-leaves
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto value = [&]() -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "missing value for " << a << "\n";
        std::exit(2);
      }
      return argv[++i];
    };
    auto files = [&](std::vector<std::string>& v) {
      while (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) v.push_back(argv[++i]);
    };
    if (a == "--leaves")
      leaves_path = value();
    else if (a == "--leave-deltas")
      deltas_path = value();
    else if (a == "--half-a")
      files(half_a);
    else if (a == "--half-b")
      files(half_b);
    else if (a == "--target")
      target = value()[0];
    else if (a == "--cap")
      cap = std::stod(value());
    else if (a == "--smooth6")
      smooth6 = true;
    else if (a == "--min-delta")
      min_delta = std::stod(value());
    else if (a == "--out")
      out_path = value();
    else {
      std::cerr << "usage: " << argv[0]
                << " --leaves LEAVES [--leave-deltas CSV] --half-a FILE... --half-b FILE..."
                   " [--target a|b|c] [--cap 3] [--min-delta 0.05] --out DELTAS.csv\n";
      return 2;
    }
  }
  if (leaves_path.empty() || half_a.empty() || half_b.empty() || out_path.empty() ||
      (target != 'a' && target != 'b' && target != 'c')) {
    std::cerr << "--leaves, --half-a, --half-b and --out are required; --target is a, b or c\n";
    return 2;
  }
  Leaves prior = Leaves::FromFile(leaves_path);
  if (!deltas_path.empty()) prior.AddDeltas(deltas_path);
  const Fit fa = Accumulate(half_a, target), fb = Accumulate(half_b, target);
  // The whole data is the two halves together: their sums add up.
  Fit full;
  full.samples = fa.samples + fb.samples;
  full.racks = fa.racks + fb.racks;
  full.noise =
      full.samples > 0 ? (fa.noise * fa.samples + fb.noise * fb.samples) / full.samples : 0;
  full.empty_mean = (fa.empty_mean + fb.empty_mean) / 2;
  full.acc = fa.acc;
  for (const auto& [key, a] : fb.acc) {
    Acc& f = full.acc[key];
    f.w += a.w;
    f.s += a.s;
    f.q += a.q;
  }

  auto leave_of = [](std::uint64_t key) {
    // Leaves::Key: 5 bits a tile (tile + 1), first tile highest, padded to 12 tiles.
    Rack r;
    for (int s = 11; s >= 0; --s) {
      const int code = static_cast<int>((key >> (5 * s)) & 31);
      if (code) r.Add(Tile(code - 1));
    }
    return r;
  };
  auto value_of = [](const Fit& f, const Acc& a) { return a.s / a.w - f.empty_mean; };

  // Target b is a change in win chance: scale it to points on well-measured leaves.
  double scale = 1;
  if (target == 'b') {
    double sxy = 0, sxx = 0;
    for (const auto& [key, a] : full.acc) {
      if (a.w * a.w / a.q < 2000) continue;  // effective samples
      const double x = value_of(full, a), y = prior.Value(leave_of(key));
      sxy += x * y;
      sxx += x * x;
    }
    scale = sxx > 0 ? sxy / sxx : 0;
  }

  // True spread of the differences: covariance of the two halves' differences.
  double cov = 0, ma = 0, mb = 0;
  long common = 0;
  std::vector<std::pair<double, double>> pairs;
  for (const auto& [key, a] : fa.acc) {
    const auto it = fb.acc.find(key);
    if (it == fb.acc.end()) continue;
    const double p = prior.Value(leave_of(key));
    pairs.emplace_back(scale * value_of(fa, a) - p, scale * value_of(fb, it->second) - p);
  }
  for (const auto& [x, y] : pairs) {
    ma += x;
    mb += y;
  }
  common = static_cast<long>(pairs.size());
  if (common > 1) {
    ma /= common;
    mb /= common;
    for (const auto& [x, y] : pairs) cov += (x - ma) * (y - mb);
    cov /= common - 1;
  }
  const double tau2 = std::max(0.0, cov);
  double sxx = 0, syy = 0, sxy = 0;
  for (const auto& [x, y] : pairs) {
    sxx += (x - ma) * (x - ma);
    syy += (y - mb) * (y - mb);
    sxy += (x - ma) * (y - mb);
  }
  const double split_r = sxx > 0 && syy > 0 ? sxy / std::sqrt(sxx * syy) : 0;
  // The same, weighting each leave by how much it is measured (its effective
  // samples, a proxy for how often it is kept): the common leaves matter most.
  double wsum = 0, wxx = 0, wyy = 0, wxy = 0;
  {
    std::size_t i = 0;
    for (const auto& [key, a] : fa.acc) {
      const auto it = fb.acc.find(key);
      if (it == fb.acc.end()) continue;
      const double w = a.w * a.w / a.q + it->second.w * it->second.w / it->second.q;
      const auto& [x, y] = pairs[i++];
      wsum += w;
      wxx += w * (x - ma) * (x - ma);
      wyy += w * (y - mb) * (y - mb);
      wxy += w * (x - ma) * (y - mb);
    }
  }
  const double weighted_r = wxx > 0 && wyy > 0 ? wxy / std::sqrt(wxx * wyy) : 0;

  // --smooth6. A 6-tile leave completes to only some 20 racks, so its own
  // value is noisy; its 5-tile sub-leaves are measured far better. Predict it
  // from them (v6 ~ a + b * mean of its sub-leaves + c * mean of its tiles,
  // fitted on well-measured 6-tile leaves), then keep of its own deviation from
  // that prediction the share its data supports: empirical Bayes again, the
  // true spread around the prediction being the covariance of the two halves.
  // It is used only if it predicts half b from half a better than the plain fit.
  std::unordered_map<std::uint64_t, double> smoothed;  // final value of 6-tile leaves
  if (smooth6) {
    auto value_map = [&](const Fit& f) {
      std::unordered_map<std::uint64_t, double> m;
      m.reserve(f.acc.size());
      for (const auto& [key, a] : f.acc) m[key] = scale * value_of(f, a);
      return m;
    };
    const auto vf = value_map(full), va = value_map(fa), vb = value_map(fb);
    auto noise_of = [&](const Fit& f, const Acc& a) {
      return scale * scale * f.noise * a.q / (a.w * a.w);
    };
    // Features of each 6-tile leave from the whole data's shorter leaves.
    struct Six {
      std::uint64_t key;
      double m5, s1;
    };
    std::vector<Six> sixes;
    for (const auto& [key, a] : full.acc) {
      const Rack leave = leave_of(key);
      if (leave.Size() != 6) continue;
      double m5 = 0, s1 = 0;
      int n5 = 0;
      for (int t = 0; t < kNumTileTypes; ++t) {
        if (!leave.Count(Tile(t))) continue;
        Rack sub = leave;
        sub.Remove(Tile(t));
        const auto it = vf.find(Leaves::Key(sub));
        if (it != vf.end()) {
          m5 += it->second * leave.Count(Tile(t));
          n5 += leave.Count(Tile(t));
        }
        Rack one;
        one.Add(Tile(t));
        const auto jt = vf.find(Leaves::Key(one));
        if (jt != vf.end()) s1 += jt->second * leave.Count(Tile(t));
      }
      if (n5 == 0) continue;
      sixes.push_back({key, m5 / n5, s1 / 6});
    }
    // Weighted least squares (weight 1 / noise) of v6 on (1, m5, s1).
    double A[3][3] = {}, B[3] = {};
    for (const Six& x : sixes) {
      const Acc& a = full.acc.at(x.key);
      const double w = 1 / std::max(noise_of(full, a), 1e-6);
      const double f[3] = {1, x.m5, x.s1}, y = vf.at(x.key);
      for (int i = 0; i < 3; ++i) {
        B[i] += w * f[i] * y;
        for (int j = 0; j < 3; ++j) A[i][j] += w * f[i] * f[j];
      }
    }
    // Solve the 3x3 system by Cramer's rule.
    auto det3 = [](double m[3][3]) {
      return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
             m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
             m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    };
    const double d = det3(A);
    double coef[3] = {0, 0, 0};
    if (std::abs(d) > 1e-12) {
      for (int k = 0; k < 3; ++k) {
        double m[3][3];
        for (int i = 0; i < 3; ++i)
          for (int j = 0; j < 3; ++j) m[i][j] = j == k ? B[i] : A[i][j];
        coef[k] = det3(m) / d;
      }
    }
    // True spread of the deviations from the prediction, from the halves.
    std::vector<std::pair<double, double>> dev;
    for (const Six& x : sixes) {
      const auto ia = va.find(x.key), ib = vb.find(x.key);
      if (ia == va.end() || ib == vb.end()) continue;
      const double pred = coef[0] + coef[1] * x.m5 + coef[2] * x.s1;
      dev.emplace_back(ia->second - pred, ib->second - pred);
    }
    double mx = 0, my = 0, c = 0;
    for (const auto& [x, y] : dev) {
      mx += x;
      my += y;
    }
    if (dev.size() > 1) {
      mx /= dev.size();
      my /= dev.size();
      for (const auto& [x, y] : dev) c += (x - mx) * (y - my);
      c /= dev.size() - 1;
    }
    const double tau6 = std::max(0.0, c);
    // Cross-check: predict half b's value from half a, smoothed or plain.
    double err_smooth = 0, err_plain = 0;
    long checked = 0;
    for (const Six& x : sixes) {
      const auto ia = fa.acc.find(x.key);
      const auto ib = vb.find(x.key);
      if (ia == fa.acc.end() || ib == vb.end()) continue;
      const double a_val = scale * value_of(fa, ia->second), na = noise_of(fa, ia->second);
      const double pred = coef[0] + coef[1] * x.m5 + coef[2] * x.s1;
      const double p0 = prior.Value(leave_of(x.key));
      const double ls = tau6 > 0 ? tau6 / (tau6 + na) : 0, lp = tau2 > 0 ? tau2 / (tau2 + na) : 0;
      err_smooth += std::pow(ib->second - (pred + ls * (a_val - pred)), 2);
      err_plain += std::pow(ib->second - (p0 + lp * (a_val - p0)), 2);
      ++checked;
    }
    std::printf(
        "smooth6: %zu six-tile leaves, v6 = %.3f + %.3f m5 + %.3f s1, true sd around it %.3f; "
        "half-b error smoothed %.3f vs plain %.3f over %ld leaves\n",
        sixes.size(), coef[0], coef[1], coef[2], std::sqrt(tau6),
        checked ? std::sqrt(err_smooth / checked) : 0.0,
        checked ? std::sqrt(err_plain / checked) : 0.0, checked);
    if (checked && err_smooth < err_plain) {
      for (const Six& x : sixes) {
        const Acc& a = full.acc.at(x.key);
        const double pred = coef[0] + coef[1] * x.m5 + coef[2] * x.s1;
        const double lam = tau6 > 0 ? tau6 / (tau6 + noise_of(full, a)) : 0;
        smoothed[x.key] = pred + lam * (vf.at(x.key) - pred);
      }
      std::printf("smooth6 used\n");
    } else {
      std::printf("smooth6 not used: it does not predict half b better\n");
    }
  }

  std::ofstream out(out_path);
  out << "leave,delta\n";
  long written = 0, capped = 0;
  double abs_sum = 0, lambda_sum = 0;
  for (const auto& [key, a] : full.acc) {
    const Rack leave = leave_of(key);
    const double p = prior.Value(leave);
    const double d = scale * value_of(full, a) - p;
    const double noise = scale * scale * full.noise * a.q / (a.w * a.w);
    const double lambda = tau2 > 0 ? tau2 / (tau2 + noise) : 0;
    lambda_sum += lambda;
    double delta = lambda * d;
    if (const auto it = smoothed.find(key); it != smoothed.end()) delta = it->second - p;
    if (std::abs(delta) > cap) {
      delta = std::copysign(cap, delta);
      ++capped;
    }
    if (std::abs(delta) < min_delta) continue;
    std::string text = leave.ToString();
    out << text << "," << delta << "\n";
    ++written;
    abs_sum += std::abs(delta);
  }
  std::printf(
      "target %c: %ld samples, %ld racks, %zu leaves with data; empty-bag rack mean %.3f; "
      "noise sd %.2f\n",
      target, full.samples, full.racks, full.acc.size(), full.empty_mean, std::sqrt(full.noise));
  std::printf(
      "scale %.3f; halves: %ld common leaves, split-half r %.3f (weighted by use %.3f), true sd "
      "of differences %.3f\n",
      scale, common, split_r, weighted_r, std::sqrt(tau2));
  std::printf(
      "deltas written %ld (|delta| >= %.2f), mean |delta| %.3f, capped %ld, mean shrink %.3f\n",
      written, min_delta, written ? abs_sum / written : 0.0, capped,
      full.acc.empty() ? 0.0 : lambda_sum / full.acc.size());
  return 0;
}
