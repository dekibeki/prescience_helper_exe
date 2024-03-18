#include <prescience_helper/sim/specs/aug.hpp>
#include <prescience_helper/sim/spells.hpp>
#include <prescience_helper/sim/helpers.hpp>

namespace sim = prescience_helper::sim;

namespace {
  namespace SPELL {
    constexpr std::uint64_t living_flame = 361469;
    constexpr std::uint64_t eruption = 395160;
    constexpr std::uint64_t trembling_earth_buff = 424368;
    constexpr std::uint64_t trembling_earth_damage = 424428;
    constexpr std::uint64_t upheaval = 396288;
  }
  namespace TALENT {
    constexpr std::uint32_t enkindled = 115603;
    constexpr std::uint32_t instinctive_arcana = 115619;
    constexpr std::uint32_t ricochetting_pyroclast = 115507;
    constexpr std::uint32_t unyielding_domain = 115501;
    constexpr std::uint32_t tectonic_locus = 115500;
  }
  
  using School = clogparser::Spell_schools::School;

  static constexpr School volcanic = School::fire | School::nature;

  const sim::Combat_stats base_stats{
    0, 8,
    0, 0.05,
    0, 0,
    0, 1
  };

  struct Aug_player_state final : public sim::Player_state {
    Aug_player_state(clogparser::events::Combatant_info const& c_info) :
      Player_state(c_info, clogparser::Attribute_rating::intelligence) {

      current_stats += base_stats;

      for (auto const& talent : c_info.talents) {
        apply_talent_(talent);
      }
    }

    sim::Damage impact(std::uint64_t spell_id, bool crit, sim::Target_state const& target, std::int64_t historical_damage_done) const override {
      switch (spell_id) {
      case SPELL::upheaval: return sim::calc_spell(spell_id, crit, *this, target, historical_damage_done, spell_scaling.upheaval);
      default: return Player_state::impact(spell_id, crit, target, historical_damage_done);
      }
    }

    virtual void handle_aura(sim::Unit caster, std::uint64_t spell_id, std::uint8_t new_stacks, std::uint8_t old_stacks) override {
      switch (spell_id) {
      default:
        Player_state::handle_aura(caster, spell_id, new_stacks, old_stacks);
      }
    }

    void apply_talent_(clogparser::events::Combatant_info::Talent const& talent) {
      switch (talent.trait_node_entry_id) {
      case TALENT::enkindled:
        break; 
      case TALENT::instinctive_arcana:
        break;
      case TALENT::ricochetting_pyroclast:
        break;
      case TALENT::unyielding_domain:
        spell_scaling.upheaval.crit_chance_add += 0.1;
        break;
      case TALENT::tectonic_locus:
        break;
      }
    }



    struct {
      sim::Damage_amp upheaval;
    } spell_scaling;
  };
}

std::unique_ptr<sim::Player_state> sim::specs::create_aug(clogparser::events::Combatant_info const& c_info) {
  return std::make_unique<Aug_player_state>(c_info);
}