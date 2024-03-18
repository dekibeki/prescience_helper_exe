#include <prescience_helper/sim.hpp>
#include <prescience_helper/sim/dbc_constants.hpp>
#include <prescience_helper/sim/spells.hpp>
#include <prescience_helper/sim/helpers.hpp>
#include <prescience_helper/sim/specs/aug.hpp>
#include <prescience_helper/sim/helpers.hpp>
#include <prescience_helper/sim/dbc/rand_prop_points.hpp>
#include <prescience_helper/sim/dbc/item_sparse.hpp>
#include <prescience_helper/sim/dbc/combat_ratings_mult_by_ilvl.hpp>
#include <cassert>

namespace sim = prescience_helper::sim;

namespace {
  namespace SPELL {
    constexpr std::uint64_t mark_of_the_wild = 1126;
    constexpr std::uint64_t sophic_devotion = 390224;
    constexpr std::uint64_t well_fed = 396092;
    constexpr std::uint64_t draconic_augmentation = 393438;
    constexpr std::uint64_t rallied_to_victory = 378139;
    constexpr std::uint64_t kindled_soul = 268998; //balefire branch
    constexpr std::uint64_t prescience_buff = 410089; //buff
    constexpr std::uint64_t ebon_might = 395152;
    constexpr std::uint64_t shifting_sands = 413984;
  }

  namespace COEFFICIENT {
    constexpr float rallied_to_victory = 0.74680960178;
    constexpr float kindled_soul = 0.03685048223;
  }

  namespace ITEM {
    constexpr std::uint64_t allied_chestplate_of_generosity = 190519;
    constexpr std::uint64_t allied_heartwarming_fur_coat = 193453;
    constexpr std::uint64_t allied_legguards_of_sansok_khan = 193464;
    constexpr std::uint64_t allied_wristguards_of_time_dilation = 193530;
    constexpr std::uint64_t balefire_branch = 159630;
  }

  double base_primary(clogparser::SpecId spec) {
    return 2089;
    /*switch (spec) {
    case clogparser::SpecId::dk_blood: return 2089;
    case clogparser::SpecId::dk_frost: return 2089;
    case clogparser::SpecId::dk_unholy: return 2089;
    case clogparser::SpecId::dk_unspecced: return 2089;

    case clogparser::SpecId::dh_havoc: return 2089;
    case clogparser::SpecId::dh_vengeance: return 2089;
    case clogparser::SpecId::dh_unspecced: return 2089;

        case clogparser::SpecId::druid_balance:
        case clogparser::SpecId::druid_feral:
        case clogparser::SpecId::druid_guardian:
        case clogparser::SpecId::druid_restoration:
        case clogparser::SpecId::druid_unspecced:

        case clogparser::SpecId::evoker_devastation:
        case clogparser::SpecId::evoker_preservation:
        case clogparser::SpecId::evoker_augmentation:
        case clogparser::SpecId::evoker_unspecced:

        case clogparser::SpecId::hunter_beast_mastery:
        case clogparser::SpecId::hunter_marksmanship:
        case clogparser::SpecId::hunter_survival:
        case clogparser::SpecId::hunter_unspecced:

        case clogparser::SpecId::mage_arcane:
        case clogparser::SpecId::mage_fire:
        case clogparser::SpecId::mage_frost:
        case clogparser::SpecId::mage_unspecced:

        case clogparser::SpecId::monk_brewmaster:
        case clogparser::SpecId::monk_windwalker:
        case clogparser::SpecId::monk_mistweaver:
        case clogparser::SpecId::monk_unspecced:

        case clogparser::SpecId::paladin_holy:
        case clogparser::SpecId::paladin_protection:
        case clogparser::SpecId::paladin_retribution:
        case clogparser::SpecId::paladin_unspecced:

        case clogparser::SpecId::priest_discipline:
        case clogparser::SpecId::priest_holy:
        case clogparser::SpecId::priest_shadow:
        case clogparser::SpecId::priest_unspecced:

        case clogparser::SpecId::rogue_assassination:
        case clogparser::SpecId::rogue_outlaw:
        case clogparser::SpecId::rogue_subtlety:
        case clogparser::SpecId::rogue_unspecced:

        case clogparser::SpecId::shaman_elemental:
        case clogparser::SpecId::shaman_enhancement:
        case clogparser::SpecId::shaman_restoration:
        case clogparser::SpecId::shaman_unspecced:

        case clogparser::SpecId::warlock_afflication:
        case clogparser::SpecId::warlock_demonology:
        case clogparser::SpecId::warlock_destruction:
        case clogparser::SpecId::warlock_unspecced:

        case clogparser::SpecId::warrior_arms:
        case clogparser::SpecId::warrior_fury:
        case clogparser::SpecId::warrior_protection:
        case clogparser::SpecId::warrior_unspecced:
    }*/
  }

  struct Nyi_spec final : sim::Player_state {
    Nyi_spec(clogparser::events::Combatant_info const& c_info, clogparser::Attribute_rating primary) :
      Player_state(c_info, primary) {
      current_stats[sim::Combat_stat::crit_val] += 0.05;
      current_stats[sim::Combat_stat::mastery_val] += 8;
    }
  };

  std::unique_ptr<sim::Player_state> create_nyi(clogparser::events::Combatant_info const& c_info, clogparser::Attribute_rating primary) {
    return std::make_unique<Nyi_spec>(c_info, primary);
  }

  struct Weapon_enchants {
    std::optional<std::int32_t> mh;
    std::optional<std::int32_t> oh;
  };

  double handle_sophic_devotion(sim::Player_state const& player) {
    double adding = 0;
    int found = 0;
    for (auto const& item : player.items) {
      if (item.permanent_enchant_id) {
        switch (*item.permanent_enchant_id) {
        case 6641: //r1
          ++found;
          adding += 783;
          break;
        case 6642: //r2
          ++found;
          adding += 857;
          break;
        case 6643: //r3
          ++found;
          adding += 932;
          break;
        }
      }
    }
    assert(found > 0);
    return adding / found;
  }

  float item_rating_mult(clogparser::Item_slot slot, std::uint16_t ilvl) {
    const auto found = sim::dbc::COMBAT_RATINGS_MULT_BY_ILVL.find(ilvl);
    if (found == sim::dbc::COMBAT_RATINGS_MULT_BY_ILVL.end()) {
      assert(false);
      return 0;
    }
    switch (slot) {
    case clogparser::Item_slot::bow: return found->second.weapon;
    case clogparser::Item_slot::ranged_2: return found->second.weapon;
    case clogparser::Item_slot::two_hand: return found->second.weapon;
    case clogparser::Item_slot::off_hand_weapon: return found->second.weapon;
    case clogparser::Item_slot::one_hand: return found->second.weapon;
    case clogparser::Item_slot::main_hand: return found->second.weapon;

    case clogparser::Item_slot::head: return found->second.armor;
    case clogparser::Item_slot::chest: return found->second.armor;
    case clogparser::Item_slot::robe: return found->second.armor;
    case clogparser::Item_slot::legs: return found->second.armor;
    case clogparser::Item_slot::shoulder: return found->second.armor;
    case clogparser::Item_slot::waist: return found->second.armor;
    case clogparser::Item_slot::feet: return found->second.armor;
    case clogparser::Item_slot::hands: return found->second.armor;
    case clogparser::Item_slot::wrist: return found->second.armor;
    case clogparser::Item_slot::back: return found->second.armor;
    case clogparser::Item_slot::off_hand: return found->second.armor;
    case clogparser::Item_slot::shield: return found->second.armor;

    case clogparser::Item_slot::trinket: return found->second.trinket;

    case clogparser::Item_slot::neck: return found->second.jewelry;
    case clogparser::Item_slot::finger: return found->second.jewelry;
    default:
      assert(false);
      return 0;
    }
  }

  sim::Combat_stats get_item_stats(clogparser::Attribute_rating primary, std::uint64_t item_id, std::uint16_t ilvl) {
    sim::Combat_stats returning;
    const auto found_item = sim::dbc::ITEM_SPARSE.find(item_id);
    if (found_item == sim::dbc::ITEM_SPARSE.end()) {
      assert(false);
      return returning;
    }
    const auto found_rand_prop_point = sim::dbc::RAND_PROP_POINT.find(ilvl);
    if (found_rand_prop_point == sim::dbc::RAND_PROP_POINT.end()) {
      assert(false);
      return returning;
    }

    std::uint8_t prop_point_id = 0;

    switch (found_item->second.slot) {
    case clogparser::Item_slot::bow: prop_point_id = 0; break;
    case clogparser::Item_slot::ranged_2: prop_point_id = 0; break;
    case clogparser::Item_slot::two_hand: prop_point_id = 0; break;
    case clogparser::Item_slot::head: prop_point_id = 0; break;
    case clogparser::Item_slot::chest: prop_point_id = 0; break;
    case clogparser::Item_slot::robe: prop_point_id = 0; break;
    case clogparser::Item_slot::legs: prop_point_id = 0; break;

    case clogparser::Item_slot::shoulder: prop_point_id = 1; break;
    case clogparser::Item_slot::waist: prop_point_id = 1; break;
    case clogparser::Item_slot::feet: prop_point_id = 1; break;
    case clogparser::Item_slot::hands: prop_point_id = 1; break;
    case clogparser::Item_slot::trinket: prop_point_id = 1; break;

    case clogparser::Item_slot::neck: prop_point_id = 2; break;
    case clogparser::Item_slot::wrist: prop_point_id = 2; break;
    case clogparser::Item_slot::finger: prop_point_id = 2; break;
    case clogparser::Item_slot::back: prop_point_id = 2; break;

    case clogparser::Item_slot::off_hand: prop_point_id = 3; break;
    case clogparser::Item_slot::off_hand_weapon: prop_point_id = 3; break;
    case clogparser::Item_slot::one_hand: prop_point_id = 3; break;
    case clogparser::Item_slot::main_hand: prop_point_id = 3; break;
    case clogparser::Item_slot::shield: prop_point_id = 3; break;

    case clogparser::Item_slot::shirt: return returning; //assume shirt will have no stats
    case clogparser::Item_slot::tabard: return returning; //assume tabard will have no stats

    default: assert(false); return returning;
    }

    const auto budget = found_rand_prop_point->second.budget[prop_point_id];

    for (std::size_t i = 0; i < 10; ++i) {
      const auto amount = budget * found_item->second.budget[i].amount * 0.0001;
      switch (found_item->second.budget[i].type) {
      case sim::dbc::Item_sparse::Stat::Type::agility:
        if (primary == clogparser::Attribute_rating::agility) {
          returning[sim::Combat_stat::primary] += amount;
        }
        break;
      case sim::dbc::Item_sparse::Stat::Type::strength:
        if (primary == clogparser::Attribute_rating::strength) {
          returning[sim::Combat_stat::primary] += amount;
        }
        break;
      case sim::dbc::Item_sparse::Stat::Type::intellect:
        if (primary == clogparser::Attribute_rating::intelligence) {
          returning[sim::Combat_stat::primary] += amount;
        }
        break;
      case sim::dbc::Item_sparse::Stat::Type::critical_strike:
        returning[sim::Combat_stat::crit_rating] += amount * item_rating_mult(found_item->second.slot, ilvl);
        break;
      case sim::dbc::Item_sparse::Stat::Type::versatility:
        returning[sim::Combat_stat::vers_rating] += amount * item_rating_mult(found_item->second.slot, ilvl);
        break;
      case sim::dbc::Item_sparse::Stat::Type::mastery:
        returning[sim::Combat_stat::mastery_rating] += amount * item_rating_mult(found_item->second.slot, ilvl);
        break;
      case sim::dbc::Item_sparse::Stat::Type::agility_or_strength_or_intellect:
        returning[sim::Combat_stat::primary] += amount;
        break;
      case sim::dbc::Item_sparse::Stat::Type::agility_or_strength:
        if (primary == clogparser::Attribute_rating::strength || primary == clogparser::Attribute_rating::agility) {
          returning[sim::Combat_stat::primary] += amount;
        }
        break;
      case sim::dbc::Item_sparse::Stat::Type::agility_or_intellect:
        if (primary == clogparser::Attribute_rating::agility || primary == clogparser::Attribute_rating::intelligence) {
          returning[sim::Combat_stat::primary] += amount;
        }
        break;
      case sim::dbc::Item_sparse::Stat::Type::strength_or_intellect:
        if (primary == clogparser::Attribute_rating::strength || primary == clogparser::Attribute_rating::intelligence) {
          returning[sim::Combat_stat::primary] += amount;
        }
        break;
      }
    }

    return returning;
  }

  sim::Aug_buff& get_aug_buff_for(sim::Player_state& player, sim::Player_state const& aug) {
    const auto found = std::find_if(player.aug_buffs.begin(), player.aug_buffs.end(), [aug](sim::Aug_buff const& buff) {
      return buff.aug == &aug;
      });

    if (found != player.aug_buffs.end()) {
      return *found;
    }

    player.aug_buffs.emplace_back();
    player.aug_buffs.back().aug = &aug;
    return player.aug_buffs.back();
  }

  double calc_damage(sim::Damage const& damage, sim::Combat_stats const& damager, sim::Combat_stats const& aug, bool ebon_might, bool shifting_sands, bool prescience, bool fate_mirror) {
    double vers_scaling = 1 + calc_vers(damager);
    double primary = damager[sim::Combat_stat::primary];
    double crit_chance = calc_crit(damager) + damage.amp.crit_chance_add;
    double extra_scaling = 1;

    //aug_int
    if (ebon_might) {
      primary += aug[sim::Combat_stat::primary] * aug[sim::Combat_stat::primary_scaling] * 0.065;
    }
    //shifting sands
    if (shifting_sands) {
      vers_scaling += sim::SHIFTING_SANDS_MASTERY_MULTIPLER * sim::calc_mastery(aug);
    }
    //prescience
    if (prescience) {
      crit_chance += 0.03;
      if (fate_mirror) {
        extra_scaling *= 1.015;
      }
    }

    //clamp crit
    crit_chance = std::min(1.0, crit_chance);
    //calc crit scaling
    const double crit_scaling = 1 * (1 - crit_chance) + 2 * damage.amp.crit_amp * crit_chance;

    return damage.base_scaling * vers_scaling * (damage.scales_with_primary ? primary : 1) * crit_scaling * extra_scaling;
  }
}

sim::Target_state::Target_state(std::string_view guid) :
  guid(guid) {

}

std::uint8_t sim::Target_state::stacks_of(std::uint64_t spell_id) const noexcept {
  const auto found = auras.find(spell_id);
  if (found == auras.end()) {
    return 0;
  } else {
    return found->second;
  }
}

void sim::Target_state::aura_changed(Unit caster, std::uint64_t spell_id, std::uint8_t stacks) {
  const auto found = auras.find(spell_id);

  std::uint8_t prev_stacks = 0;
  if (found != auras.end()) {
    prev_stacks = found->second;
    if (stacks == 0) {
      auras.erase(found);
    } else if(stacks == prev_stacks){
      return; //from stacks -> stacks?
    } else {
      found->second = stacks;
    }
    handle_aura(caster, spell_id, stacks, prev_stacks);
  } else {
    if (stacks == 0) {
      return; //from 0 -> 0?
    }
    auras[spell_id] = stacks;
    handle_aura(caster, spell_id, stacks, 0);
  }
}

void sim::Target_state::handle_aura(Unit caster, std::uint64_t spell_id, std::uint8_t new_stacks, std::uint8_t old_stacks) { }

sim::Calced_damage sim::Damage::calc(Combat_stats const& damager, Combat_stats const& aug, bool fate_mirror) const noexcept {
  Calced_damage returning;
  returning.base = calc_damage(*this, damager, aug, false, false, false, false);

  returning.with_ebon_mult = calc_damage(*this, damager, aug, true, false, false, false) / returning.base;
  returning.with_shifting_sands_mult = calc_damage(*this, damager, aug, false, true, false, false) / returning.base;
  returning.with_prescience_mult = calc_damage(*this, damager, aug, false, false, true, fate_mirror) / returning.base;

  return returning;
}

std::unique_ptr<sim::Player_state> sim::Player_state::create(clogparser::events::Combatant_info const& c_info) {
  switch (c_info.current_spec_id) {
    //idk what to do with this
  case clogparser::SpecId::invalid: return create_nyi(c_info, clogparser::Attribute_rating::strength);

  case clogparser::SpecId::dk_blood: return create_nyi(c_info, clogparser::Attribute_rating::strength);
  case clogparser::SpecId::dk_frost: return create_nyi(c_info, clogparser::Attribute_rating::strength);
  case clogparser::SpecId::dk_unholy: return create_nyi(c_info, clogparser::Attribute_rating::strength);
  case clogparser::SpecId::dk_unspecced: return create_nyi(c_info, clogparser::Attribute_rating::strength);

  case clogparser::SpecId::dh_havoc: return create_nyi(c_info, clogparser::Attribute_rating::agility);
  case clogparser::SpecId::dh_vengeance: return create_nyi(c_info, clogparser::Attribute_rating::agility);
  case clogparser::SpecId::dh_unspecced: return create_nyi(c_info, clogparser::Attribute_rating::agility);

  case clogparser::SpecId::druid_balance: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::druid_feral: return create_nyi(c_info, clogparser::Attribute_rating::agility);
  case clogparser::SpecId::druid_guardian: return create_nyi(c_info, clogparser::Attribute_rating::agility);
  case clogparser::SpecId::druid_restoration: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::druid_unspecced: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);

  case clogparser::SpecId::evoker_devastation: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::evoker_preservation: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::evoker_augmentation: return specs::create_aug(c_info);
  case clogparser::SpecId::evoker_unspecced: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);

  case clogparser::SpecId::hunter_beast_mastery: return create_nyi(c_info, clogparser::Attribute_rating::agility);
  case clogparser::SpecId::hunter_marksmanship: return create_nyi(c_info, clogparser::Attribute_rating::agility);
  case clogparser::SpecId::hunter_survival: return create_nyi(c_info, clogparser::Attribute_rating::agility);
  case clogparser::SpecId::hunter_unspecced: return create_nyi(c_info, clogparser::Attribute_rating::agility);

  case clogparser::SpecId::mage_arcane: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::mage_fire: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::mage_frost: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::mage_unspecced: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);

  case clogparser::SpecId::monk_brewmaster: return create_nyi(c_info, clogparser::Attribute_rating::agility);
  case clogparser::SpecId::monk_windwalker: return create_nyi(c_info, clogparser::Attribute_rating::agility);
  case clogparser::SpecId::monk_mistweaver: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::monk_unspecced: return create_nyi(c_info, clogparser::Attribute_rating::agility);

  case clogparser::SpecId::paladin_holy: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::paladin_protection: return create_nyi(c_info, clogparser::Attribute_rating::strength);
  case clogparser::SpecId::paladin_retribution: return create_nyi(c_info, clogparser::Attribute_rating::strength);
  case clogparser::SpecId::paladin_unspecced: return create_nyi(c_info, clogparser::Attribute_rating::strength);

  case clogparser::SpecId::priest_discipline: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::priest_holy: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::priest_shadow: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::priest_unspecced: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);

  case clogparser::SpecId::rogue_assassination: return create_nyi(c_info, clogparser::Attribute_rating::agility);
  case clogparser::SpecId::rogue_outlaw: return create_nyi(c_info, clogparser::Attribute_rating::agility);
  case clogparser::SpecId::rogue_subtlety: return create_nyi(c_info, clogparser::Attribute_rating::agility);
  case clogparser::SpecId::rogue_unspecced: return create_nyi(c_info, clogparser::Attribute_rating::agility);

  case clogparser::SpecId::shaman_elemental: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::shaman_enhancement: return create_nyi(c_info, clogparser::Attribute_rating::agility);
  case clogparser::SpecId::shaman_restoration: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::shaman_unspecced: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);

  case clogparser::SpecId::warlock_afflication: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::warlock_demonology: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::warlock_destruction: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);
  case clogparser::SpecId::warlock_unspecced: return create_nyi(c_info, clogparser::Attribute_rating::intelligence);

  case clogparser::SpecId::warrior_arms: return create_nyi(c_info, clogparser::Attribute_rating::strength);
  case clogparser::SpecId::warrior_fury: return create_nyi(c_info, clogparser::Attribute_rating::strength);
  case clogparser::SpecId::warrior_protection: return create_nyi(c_info, clogparser::Attribute_rating::strength);
  case clogparser::SpecId::warrior_unspecced: return create_nyi(c_info, clogparser::Attribute_rating::strength);

  default:
    throw std::exception("Unknown spec");
  }
}

sim::Player_state::Player_state(clogparser::events::Combatant_info const& info, clogparser::Attribute_rating  primary_stat) :
  Target_state(info.guid),
  spec(info.current_spec_id),
  primary_stat(primary_stat),
  talents(info.talents),
  items(info.items) {

  current_stats[Combat_stat::primary] = base_primary(info.current_spec_id);
  current_stats[Combat_stat::primary_scaling] = 1;

  for (auto const& item : info.items) {
    if (item.item_id != 0) {
      const auto got = get_item_stats(primary_stat, item.item_id, item.ilvl);
      for (Combat_stat i = Combat_stat::INITIAL; i < Combat_stat::COUNT; ++i) {
        current_stats[i] += got[i];
      }
    }
  }

  for (auto const& interesting_aura : info.interesting_auras) {
    aura_changed(this, interesting_aura.spell_id, 1);
  }
}

sim::Damage sim::Player_state::swing(bool crit, Target_state const& target, std::int64_t historical_damage_done) const {
  return calc_swing(crit, *this, target, historical_damage_done);
}
sim::Damage sim::Player_state::pet_swing(bool crit, Target_state const& target, std::int64_t historical_damage_done) const {
  return calc_swing(crit, *this, target, historical_damage_done);
}

sim::Damage sim::Player_state::impact(std::uint64_t spell_id, bool crit, Target_state const& target, std::int64_t historical_damage_done) const {
  return calc_spell(spell_id, crit, *this, target, historical_damage_done);
}

void sim::Player_state::handle_aura(Unit caster, std::uint64_t spell_id, std::uint8_t new_stacks, std::uint8_t old_stacks) {
  switch (spell_id) {
  case SPELL::mark_of_the_wild:
    current_stats[Combat_stat::vers_val] += get_aura_adder(0.03, new_stacks, old_stacks);
    break;

  case SPELL::sophic_devotion:
    current_stats[Combat_stat::primary] += get_aura_adder(handle_sophic_devotion(*this), new_stacks, old_stacks);
    break;
  case SPELL::kindled_soul:
  {
    if (primary_stat != clogparser::Attribute_rating::intelligence) {
      break;
    }
    std::uint16_t ilvl = ilvl_of_item(*this, ITEM::balefire_branch);
    if (ilvl == 0) {
      assert(false);
      break;
    }

    const auto found_rand_pop_point = dbc::RAND_PROP_POINT.find(ilvl);
    if (found_rand_pop_point == dbc::RAND_PROP_POINT.end()) {
      assert(false);
      break;
    }

    const auto per_stack = COEFFICIENT::kindled_soul * found_rand_pop_point->second.budget[0];
    current_stats[Combat_stat::primary] += get_aura_adder(per_stack, new_stacks, old_stacks);
    break;
  }
  case SPELL::rallied_to_victory:
    if (std::holds_alternative<const Player_state*>(caster)) {
      std::uint16_t ilvl = 0;
      ilvl = ilvl_of_item(*std::get<const Player_state*>(caster), ITEM::allied_chestplate_of_generosity);
      if (ilvl == 0) {
        ilvl = ilvl_of_item(*std::get<const Player_state*>(caster), ITEM::allied_legguards_of_sansok_khan);
      }
      if (ilvl == 0) {
        ilvl = ilvl_of_item(*std::get<const Player_state*>(caster), ITEM::allied_heartwarming_fur_coat);
      }
      if (ilvl == 0) {
        ilvl = ilvl_of_item(*std::get<const Player_state*>(caster), ITEM::allied_wristguards_of_time_dilation);
      }

      if (ilvl == 0) {
        assert(false);
        break;
      }

      const auto found_rand_prop_point = dbc::RAND_PROP_POINT.find(ilvl);
      if (found_rand_prop_point == dbc::RAND_PROP_POINT.end()) {
        assert(false);
        break;
      }

      current_stats[Combat_stat::vers_rating] += get_aura_adder(COEFFICIENT::rallied_to_victory * found_rand_prop_point->second.damage_replace_stat, new_stacks, old_stacks);
      break;
    }
    break;
  case SPELL::prescience_buff:
    if (std::holds_alternative<const Player_state*>(caster)) {
      get_aug_buff_for(*this, *std::get<const Player_state*>(caster)).prescience = new_stacks != 0;
    }
    break;
  case SPELL::ebon_might:
    if (std::holds_alternative<const Player_state*>(caster)) {
      get_aug_buff_for(*this, *std::get<const Player_state*>(caster)).ebon_might = new_stacks != 0;
    }
    break;
  case SPELL::shifting_sands:
    if (std::holds_alternative<const Player_state*>(caster)) {
      get_aug_buff_for(*this, *std::get<const Player_state*>(caster)).shifting_sands = new_stacks != 0;
    }
    break;
  case SPELL::well_fed:
    current_stats[sim::Combat_stat::primary] += get_aura_adder(76, new_stacks, old_stacks);
    break;
  case SPELL::draconic_augmentation:
    current_stats[sim::Combat_stat::primary] += get_aura_adder(87, new_stacks, old_stacks);
    break;
  default:
    Target_state::handle_aura(caster, spell_id, new_stacks, old_stacks);
    break;
  }
}
