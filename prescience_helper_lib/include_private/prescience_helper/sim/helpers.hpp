#pragma once

#include <prescience_helper/sim.hpp>

namespace prescience_helper::sim {

  constexpr double EBON_MIGHT_PRIMARY_SHARE = 0.065;
  constexpr double SHIFTING_SANDS_MASTERY_MULTIPLER = 0.0034;
  constexpr double PRESCIENCE_CRIT_AMOUNT = 0.03;

  constexpr double get_aura_multiplier(double modifier, std::uint8_t new_stacks, std::uint8_t old_stacks) {
    //(base*(1+old*M))*(1+delta*M/(1+old*M))
    return 1 + (static_cast<int>(new_stacks) - old_stacks) * modifier / (1 + old_stacks * modifier);
  }

  constexpr double get_aura_adder(double modifier, std::uint8_t new_stacks, std::uint8_t old_stacks) {
    return modifier * (static_cast<int>(new_stacks) - old_stacks);
  }

  std::uint16_t ilvl_of_item(Player_state const& player, std::uint64_t item_id);

  constexpr Combat_stat& operator++(Combat_stat& v) noexcept {
    using Underlying_type = std::underlying_type_t<Combat_stat>;
    v = static_cast<Combat_stat>(static_cast<Underlying_type>(v) + 1);
    return v;
  }

  double calc_mastery(Combat_stats const& stats);
  double calc_crit(Combat_stats const& stats);
  double calc_vers(Combat_stats const& stats);
}