#pragma once

#include <prescience_helper/sim.hpp>
#include <clogparser/types.hpp>

namespace prescience_helper::sim {

  template<typename ...Amps>
  Damage_amp calc_amps(Amps const&... amps) {
    Damage_amp returning;
    returning.crit_chance_add = (0 + ... + amps.crit_chance_add);
    returning.crit_amp = (1 * ... * amps.crit_amp);
    return returning;
  }

  Damage calc_swing(bool crit, Player_state const& caster, Target_state const& castee, std::int64_t historical_damage_done, Damage_amp = {});
  Damage calc_spell(std::uint64_t spell_id, bool crit, Player_state const& caster, Target_state const& castee, std::int64_t historical_damage_done, Damage_amp = {});
}