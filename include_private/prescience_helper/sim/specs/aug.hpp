#pragma once

#include <prescience_helper/sim.hpp>

namespace prescience_helper::sim::specs {
  std::unique_ptr<Player_state> create_aug(clogparser::events::Combatant_info const&);
}