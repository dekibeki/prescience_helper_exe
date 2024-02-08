#pragma once

#include <prescience_helper_lib/sim.hpp>

namespace prescience_helper_lib::internal::specs {
  std::unique_ptr<Player_state> create_aug(clogparser::events::Combatant_info const&);
}