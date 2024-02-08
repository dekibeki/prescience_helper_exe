#pragma once

#include <prescience_helper/sim.hpp>
#include <clogparser/types.hpp>

namespace prescience_helper::sim {
  constexpr double EBON_MIGHT_PRIMARY_SHARE = 0.1;
  constexpr double SHIFTING_SANDS_MASTERY_MULTIPLER = 0.34;
  constexpr double PRESCIENCE_CRIT_AMOUNT = 0.03;

  template<typename ...Amps>
  Damage_amp calc_amps(Amps const&... amps) {
    Damage_amp returning;
    returning.flat_increase = (0 + ... + amps.flat_increase);
    returning.normal_amp = (1 * ... * amps.normal_amp);
    returning.crit_amp = (1 * ... * amps.crit_amp);
    return returning;
  }

  struct Spell_scaling {
    clogparser::Spell_schools school;
    double ap_scaling = 0;
    double sp_scaling = 0;
    Damage_amp extra_amp;
  };

  Spell_result calc_spell(bool crit, Player_state const& aug, Player_state const& caster, Target_state const& castee, Spell_scaling const& cast_f);

  Spell_result unknown_spell();
}