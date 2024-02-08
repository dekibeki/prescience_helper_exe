#include <prescience_helper_lib/sim.hpp>
#include <prescience_helper_lib/dbc_constants.hpp>
#include <prescience_helper_lib/spells.hpp>
#include <prescience_helper_lib/specs/aug.hpp>
#include <cassert>

namespace internal = prescience_helper_lib::internal;

namespace {
  constexpr internal::Player_scaling NYI_class_block{
    clogparser::Attribute_rating::strength,
    0, 0,
    0, 0
  };

  struct Nyi_spec final : internal::Player_state {
    Nyi_spec(clogparser::events::Combatant_info const& c_info) :
      Player_state(c_info, NYI_class_block) {

    }

    internal::Spell_result impact(std::uint64_t spell_id, bool crit, Player_state const& aug, Target_state const& target) const override {
      return internal::unknown_spell();
    }
    internal::Spell_result tick(std::uint64_t spell_id, bool crit, Player_state const& aug, Target_state const& target) const override {
      return internal::unknown_spell();
    }
  };

  std::unique_ptr<internal::Player_state> create_nyi(clogparser::events::Combatant_info const& c_info) {
    return std::make_unique<Nyi_spec>(c_info);
  }
}

prescience_helper_lib::internal::Target_state::Target_state(std::string_view guid) :
  guid(guid) {

}

void internal::Target_state::aura_applied(std::uint64_t spell_id, std::optional<std::uint64_t> remaining_points) {
  switch (spell_id) {
  case 113746: //mystic touch
    phys_damage_taken.normal_amp *= 0.05;
    break;
  case 1490: //chaos brand
    magic_damage_taken.normal_amp *= 0.05;
    break;
  }
}
void internal::Target_state::aura_removed(std::uint64_t spell_id) {
  switch (spell_id) {
  }
}

void prescience_helper_lib::internal::Player_scaling::add_primary(Combat_stats& to, double amount) const noexcept {
  const auto adding_primary = amount * primary_scaling;
  to[Combat_stat::primary] += adding_primary;
  const auto adding_sp = sp_per_primary * adding_primary * sp_scaling;
  to[Combat_stat::spell_power] += adding_sp;
  to[Combat_stat::attack_power] = ap_per_sp * adding_sp * ap_scaling;
  const auto adding_ap = ap_per_primary * amount * ap_scaling;
  to[Combat_stat::attack_power] += adding_ap;
  to[Combat_stat::spell_power] += sp_per_ap * adding_ap * sp_scaling;
}

void prescience_helper_lib::internal::Player_scaling::add_sp_scaling(Combat_stats& to, double amount) noexcept {
  const auto new_scaling = sp_scaling + amount;
  to[Combat_stat::spell_power] *= new_scaling / sp_scaling;
  sp_scaling = new_scaling;
}

void prescience_helper_lib::internal::Player_scaling::add_ap_scaling(Combat_stats& to, double amount) noexcept {
  const auto new_scaling = ap_scaling + amount;
  to[Combat_stat::spell_power] *= new_scaling / ap_scaling;
  ap_scaling = new_scaling;
}

std::unique_ptr<prescience_helper_lib::internal::Player_state> prescience_helper_lib::internal::Player_state::create(clogparser::events::Combatant_info const& c_info) {
  switch (c_info.current_spec_id) {
  case clogparser::SpecId::dk_blood: return create_nyi(c_info);
  case clogparser::SpecId::dk_frost: return create_nyi(c_info);
  case clogparser::SpecId::dk_unholy: return create_nyi(c_info);
  case clogparser::SpecId::dk_unspecced: return create_nyi(c_info);

  case clogparser::SpecId::dh_havoc: return create_nyi(c_info);
  case clogparser::SpecId::dh_vengeance: return create_nyi(c_info);
  case clogparser::SpecId::dh_unspecced: return create_nyi(c_info);

  case clogparser::SpecId::druid_balance: return create_nyi(c_info);
  case clogparser::SpecId::druid_feral: return create_nyi(c_info);
  case clogparser::SpecId::druid_guardian: return create_nyi(c_info);
  case clogparser::SpecId::druid_restoration: return create_nyi(c_info);
  case clogparser::SpecId::druid_unspecced: return create_nyi(c_info);

  case clogparser::SpecId::evoker_devastation: return create_nyi(c_info);
  case clogparser::SpecId::evoker_preservation: return create_nyi(c_info);
  case clogparser::SpecId::evoker_augmentation: return specs::create_aug(c_info);
  case clogparser::SpecId::evoker_unspecced: return create_nyi(c_info);

  case clogparser::SpecId::hunter_beast_mastery: return create_nyi(c_info);
  case clogparser::SpecId::hunter_marksmanship: return create_nyi(c_info);
  case clogparser::SpecId::hunter_survival: return create_nyi(c_info);
  case clogparser::SpecId::hunter_unspecced: return create_nyi(c_info);

  case clogparser::SpecId::mage_arcane: return create_nyi(c_info);
  case clogparser::SpecId::mage_fire: return create_nyi(c_info);
  case clogparser::SpecId::mage_frost: return create_nyi(c_info);
  case clogparser::SpecId::mage_unspecced: return create_nyi(c_info);

  case clogparser::SpecId::monk_brewmaster: return create_nyi(c_info);
  case clogparser::SpecId::monk_windwalker: return create_nyi(c_info);
  case clogparser::SpecId::monk_mistweaver: return create_nyi(c_info);
  case clogparser::SpecId::monk_unspecced: return create_nyi(c_info);

  case clogparser::SpecId::paladin_holy: return create_nyi(c_info);
  case clogparser::SpecId::paladin_protection: return create_nyi(c_info);
  case clogparser::SpecId::paladin_retribution: return create_nyi(c_info);
  case clogparser::SpecId::paladin_unspecced: return create_nyi(c_info);

  case clogparser::SpecId::priest_discipline: return create_nyi(c_info);
  case clogparser::SpecId::priest_holy: return create_nyi(c_info);
  case clogparser::SpecId::priest_shadow: return create_nyi(c_info);
  case clogparser::SpecId::priest_unspecced: return create_nyi(c_info);

  case clogparser::SpecId::rogue_assassination: return create_nyi(c_info);
  case clogparser::SpecId::rogue_outlaw: return create_nyi(c_info);
  case clogparser::SpecId::rogue_subtlety: return create_nyi(c_info);
  case clogparser::SpecId::rogue_unspecced: return create_nyi(c_info);

  case clogparser::SpecId::shaman_elemental: return create_nyi(c_info);
  case clogparser::SpecId::shaman_enhancement: return create_nyi(c_info);
  case clogparser::SpecId::shaman_restoration: return create_nyi(c_info);
  case clogparser::SpecId::shaman_unspecced: return create_nyi(c_info);

  case clogparser::SpecId::warlock_afflication: return create_nyi(c_info);
  case clogparser::SpecId::warlock_demonology: return create_nyi(c_info);
  case clogparser::SpecId::warlock_destruction: return create_nyi(c_info);
  case clogparser::SpecId::warlock_unspecced: return create_nyi(c_info);

  case clogparser::SpecId::warrior_arms: return create_nyi(c_info);
  case clogparser::SpecId::warrior_fury: return create_nyi(c_info);
  case clogparser::SpecId::warrior_protection: return create_nyi(c_info);
  case clogparser::SpecId::warrior_unspecced: return create_nyi(c_info);

  default:
    throw std::exception("Unknown spec");
  }
}

prescience_helper_lib::internal::Player_state::Player_state(clogparser::events::Combatant_info const& info, Player_scaling const& player_scaling) :
  Target_state(info.guid),
  spec(info.current_spec_id),
  scaling(player_scaling),
  talents(info.talents) {

  scaling.add_primary(current_stats, info.stats[player_scaling.primary]);

  current_stats[Combat_stat::mastery] +=
    dbc_constants::scale_rating(info.stats[clogparser::Attribute_rating::mastery] / dbc_constants::rating_per_mastery);
  assert(info.stats[clogparser::Attribute_rating::crit_melee] == info.stats[clogparser::Attribute_rating::crit_ranged]
    && info.stats[clogparser::Attribute_rating::crit_ranged] == info.stats[clogparser::Attribute_rating::crit_spell]);
  current_stats[Combat_stat::crit] +=
    dbc_constants::scale_rating(info.stats[clogparser::Attribute_rating::crit_melee] / dbc_constants::rating_per_crit) / 100; //div by 100 to convert to [0,1]
  current_stats[Combat_stat::vers] +=
    dbc_constants::scale_rating(info.stats[clogparser::Attribute_rating::versatility_damage_done] / dbc_constants::rating_per_vers_damage) / 100; //div by 100 to convert to [0,1]
}

void internal::Player_state::aura_applied(std::uint64_t spell_id, std::optional<std::uint64_t> remaining_points) {
  switch (spell_id) {
  case 1126: //mark of the wild
    current_stats[Combat_stat::vers] += 0.03; 
    break;
  case 6673: //battle shout
    scaling.add_ap_scaling(current_stats, 0.05);
    break;
  case 1459: //arcane intellect
    scaling.primary_scaling += 0.05; //since we get base primary from combatant info instead of reconstructing it
    break;

  default:
    Target_state::aura_applied(spell_id, remaining_points);
    break;
  }
}
void internal::Player_state::aura_removed(std::uint64_t spell_id) {

}