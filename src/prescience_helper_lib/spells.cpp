#include <prescience_helper/sim/spells.hpp>

namespace sim = prescience_helper::sim;

sim::Spell_result sim::calc_spell(bool crit, Player_state const& aug, Player_state const& caster, Target_state const& castee, Spell_scaling const& scaling) {
  Spell_result res;

  for (Buffs i = Buffs::none; i < Buffs::COMBINATIONS; ++i) {
    Combat_stats stats = caster.current_stats;
    if ((i & Buffs::ebon_might) != sim::Buffs::none) {
      caster.scaling.add_primary(stats, EBON_MIGHT_PRIMARY_SHARE * aug.current_stats[Combat_stat::primary]);
    }
    if ((i & Buffs::shifting_sands) != Buffs::none) {
      stats[Combat_stat::vers] += aug.current_stats[Combat_stat::mastery] * SHIFTING_SANDS_MASTERY_MULTIPLER / 100;
    }

    Damage_amp amp;
    if (scaling.school.is(clogparser::Spell_schools::School::physical)) {
      amp = calc_amps(caster.physical_damage_done, castee.phys_damage_taken, scaling.extra_amp);
    } else {
      amp = calc_amps(caster.magic_damage_done, castee.magic_damage_taken, scaling.extra_amp);
    }
    amp.normal_amp *= (1 + caster.current_stats[Combat_stat::vers]);

    res[i] = stats[Combat_stat::attack_power] * scaling.ap_scaling + stats[Combat_stat::spell_power] * scaling.sp_scaling;

    res[i] += amp.flat_increase;
    res[i] *= amp.normal_amp;

    if (crit) {
      res[i] *= amp.crit_amp * 2;
    } else if (1 - stats[Combat_stat::crit] > 0 && (i & sim::Buffs::prescience) != sim::Buffs::none) {
      const auto helpful_amount = std::min(PRESCIENCE_CRIT_AMOUNT, 1 - stats[Combat_stat::crit]);
      res[i] *= amp.crit_amp * (1 + helpful_amount / (1 - stats[Combat_stat::crit]));
    }
  }

  return res;
}


sim::Spell_result sim::unknown_spell() {
  Spell_result returning;
  for (Buffs i = Buffs::none; i < Buffs::COMBINATIONS; ++i) {
    returning[i] = 0;
  }
  return returning;
}