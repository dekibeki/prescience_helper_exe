#pragma once
#include <cstdint>
#include <unordered_map>

namespace prescience_helper::sim::dbc {
  struct Combat_ratings_mult_by_ilvl {
    std::uint16_t ilvl;
    float armor;
    float weapon;
    float trinket;
    float jewelry;

    constexpr Combat_ratings_mult_by_ilvl(std::uint16_t ilvl, float armor, float weapon, float trinket, float jewelry) :
      ilvl(ilvl), armor(armor), weapon(weapon), trinket(trinket), jewelry(jewelry) { }
  };

  extern const std::unordered_map<std::uint16_t, Combat_ratings_mult_by_ilvl> COMBAT_RATINGS_MULT_BY_ILVL;
}

