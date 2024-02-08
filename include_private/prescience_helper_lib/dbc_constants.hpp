#pragma once

namespace prescience_helper_lib::internal {
  namespace dbc_constants {
    constexpr double rating_per_crit = 179.995151;
    constexpr double rating_per_mastery = 179.995151;
    constexpr double rating_per_vers_damage = 204.9944775;

    double scale_rating(double in) noexcept;
  }
}