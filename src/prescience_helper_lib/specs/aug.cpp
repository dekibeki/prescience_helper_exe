#include <prescience_helper_lib/specs/aug.hpp>
#include <prescience_helper_lib/spells.hpp>

namespace internal = prescience_helper_lib::internal;

namespace {
  const internal::Player_scaling aug_class_block{
    clogparser::Attribute_rating::intelligence,
    1, 0,
  };
  const internal::Combat_stats base_stats{
    0, 0,
    8, 0.05, 0, 0
  };

  double living_flame_impact(prescience_helper_lib::internal::Combat_stats const& stats) {
    return stats[internal::Combat_stat::spell_power] * 1.61;
  }

  double eruption_impact(prescience_helper_lib::internal::Combat_stats const& stats) {
    return stats[internal::Combat_stat::spell_power] * 2.8;
  }

  struct Aug_player_state final : public internal::Player_state {
    Aug_player_state(clogparser::events::Combatant_info const& c_info) :
      Player_state(c_info, aug_class_block) {

      for (internal::Combat_stat i = internal::Combat_stat::INITIAL; i < internal::Combat_stat::COUNT; ++i) {
        current_stats[i] += base_stats[i];
      }

      for (auto const& talent : c_info.talents) {
        apply_talent_(talent);
      }
      for (auto const& interesting_aura : c_info.interesting_auras) {
        aura_applied(interesting_aura.spell_id, std::nullopt);
      }
    }

    internal::Spell_result impact(std::uint64_t spell_id, bool crit, Player_state const& aug, internal::Target_state const& target) const override {
      switch (spell_id) {
      case 361469: return internal::calc_spell(crit, aug, *this, target, scaling.living_flame);
      case 395160: return internal::calc_spell(crit, aug, *this, target, scaling.eruption);
      case 396286: return internal::calc_spell(crit, aug, *this, target, scaling.upheaval);
      default: return internal::unknown_spell();
      }
    }
    internal::Spell_result tick(std::uint64_t spell_id, bool crit, Player_state const& aug, internal::Target_state const& target) const override {
      switch (spell_id) {
      default: return internal::unknown_spell();
      }
    }

    virtual void aura_applied(std::uint64_t spell_id, std::optional<std::uint64_t> remaining_points) override {
      switch (spell_id) {
      case 395296: //ebon might self buff
        scaling.living_flame.extra_amp.normal_amp *= 1.2;
        scaling.eruption.extra_amp.normal_amp *= 1.2;
        break;
      }
    }
    virtual void aura_removed(std::uint64_t spell_id) override {
      switch (spell_id) {
      case 395296: //ebon might self buff
        scaling.living_flame.extra_amp.normal_amp /= 1.2;
        scaling.eruption.extra_amp.normal_amp /= 1.2;
        break;
      }
    }

    void apply_talent_(clogparser::events::Combatant_info::Talent const& talent) {
      switch (talent.trait_node_entry_id) {
      case 115603: //Enkindled
        scaling.living_flame.extra_amp.normal_amp *= (1 + 0.03 * talent.rank);
        break; 
      case 115619: //Instinctive Arcana
        magic_damage_done.normal_amp *= (1 + 0.02 * talent.rank); //this isn't quite correct, it's only magic, but close enough for now
        break;
      case 115507: //Ricochetting pyroclast
        scaling.eruption.extra_amp.normal_amp *= 1.3; //this should cap at 150%, but we can't get target counts from logs, unless we do some batched stuff
        break;
      case 115501: //Unyielding domain
        scaling.upheaval.extra_amp.crit_change_add += 0.1;
        break;
      case 115500: //Tectonic Locus
        scaling.upheaval.extra_amp.normal_amp *= 1.5;
        break;
      }
    }

    struct {
      internal::Spell_scaling living_flame{ 0,1.61 };
      internal::Spell_scaling eruption{ 0,2.8 };
      internal::Spell_scaling upheaval{ 0,4.3 };
    } scaling;
  };
}

std::unique_ptr<internal::Player_state> internal::specs::create_aug(clogparser::events::Combatant_info const& c_info) {
  return std::make_unique<Aug_player_state>(c_info);
}