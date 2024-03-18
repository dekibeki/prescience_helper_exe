#pragma once
#include <cstdint>
#include <array>
#include <unordered_map>
namespace prescience_helper::sim::dbc {
  struct Rand_prop_point {
    std::uint32_t ilvl;
    float damage_replace_stat;
    float damage_secondary;
    std::array<float,5> budget;
  };

  extern const std::unordered_map<std::uint32_t,Rand_prop_point> RAND_PROP_POINT;
}

