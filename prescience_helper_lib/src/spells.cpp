#include <prescience_helper/sim/spells.hpp>
#include <prescience_helper/sim/dbc/spell_effect.hpp>
#include <prescience_helper/sim/dbc/spell_misc.hpp>
#include <prescience_helper/sim/helpers.hpp>

namespace sim = prescience_helper::sim;

namespace {
  sim::Damage calc_impl(bool crit, sim::Player_state const& caster, sim::Target_state const& castee, std::int64_t historical_damage_done, sim::Damage_amp scaling, bool scales_with_primary, bool can_not_crit) {
    sim::Damage returning;

    sim::Combat_stats with_aug = caster.current_stats;

    for (auto const& buff : caster.aug_buffs) {
      if (buff.prescience) {
        with_aug[sim::Combat_stat::crit_val] += 0.03;
      }
      if (buff.ebon_might) {
        with_aug[sim::Combat_stat::primary] += 0.065 * buff.aug->current_stats[sim::Combat_stat::primary] * buff.aug->current_stats[sim::Combat_stat::primary_scaling];
      }
      if (buff.shifting_sands) {
        with_aug[sim::Combat_stat::vers_val] += sim::calc_mastery(buff.aug->current_stats) * sim::SHIFTING_SANDS_MASTERY_MULTIPLER;
      }
    }

    sim::Damage_amp amp = sim::calc_amps(scaling, caster.damage_done, castee.damage_taken);

    //set base
    returning.base_scaling = historical_damage_done;

    //handle primary scaling
    returning.scales_with_primary = scales_with_primary;
    if (scales_with_primary) {
      returning.base_scaling /= with_aug[sim::Combat_stat::primary];
    } else {
      int debug_point = 0;
    }

    //handle vers scaling
    returning.base_scaling /= (1 + sim::calc_vers(with_aug));

    returning.can_not_crit = can_not_crit;
    assert(!(can_not_crit && crit));
    if (can_not_crit) {
      returning.amp.crit_amp = 1;
      returning.amp.crit_chance_add = 0;
    } else {
      //handle crit
      returning.amp = amp;
      if (crit) { //if crit take that away
        returning.base_scaling /= amp.crit_amp * 2;
      }
    }

    return returning;
  }
}

sim::Damage sim::calc_swing(bool crit, Player_state const& caster, Target_state const& castee, std::int64_t historical_damage_done, Damage_amp scaling) {
  return calc_impl(crit, caster, castee, historical_damage_done, scaling, true, false);
}

sim::Damage sim::calc_spell(std::uint64_t spell_id, bool crit, Player_state const& caster, Target_state const& castee, std::int64_t historical_damage_done, Damage_amp scaling) {
  return calc_impl(crit, caster, castee, historical_damage_done, scaling, sim::dbc::scales_with_primary(spell_id), sim::dbc::can_not_crit(spell_id));
}