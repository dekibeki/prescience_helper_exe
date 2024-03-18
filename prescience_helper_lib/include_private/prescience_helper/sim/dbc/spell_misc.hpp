#pragma once
#include <cstdint>

namespace prescience_helper::sim::dbc {
  bool can_not_crit(std::uint64_t) noexcept;
  bool allow_class_ability_procs(std::uint64_t) noexcept;
}

