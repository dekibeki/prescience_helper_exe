#include <prescience_helper/sim/dbc_constants.hpp>

#include <array>
#include <cassert>
#include <algorithm>

namespace {
  struct Scaling {
    double orig_min;
    double orig_max;
    double scaled_base;
    double scaled_per_orig;

    constexpr double use(double val) const noexcept {
      assert(orig_min <= val && val <= orig_max);

      return scaled_base + (val - orig_min) * scaled_per_orig;
    }
  };

  constexpr std::array scalings = {
    Scaling{0, 30, 0, 1},
    Scaling{30, 40, 30, 0.9},
    Scaling{40, 50, 39, 0.8},
    Scaling{50, 60, 47, 0.7},
    Scaling{60, 80, 54, 0.6},
    Scaling{80, 100, 66, 0.5},
    Scaling{100, 200, 76, 0.4}
  };
}

double prescience_helper::sim::dbc_constants::scale_rating(double in) noexcept {
  for (auto const& scaling : scalings) {
    if (scaling.orig_min <= in && in <= scaling.orig_max) {
      return scaling.use(in);
    }
  }

  const auto max_scaling = scalings.back();

  return max_scaling.use(max_scaling.orig_max);
}