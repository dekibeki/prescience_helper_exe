#pragma once

#include <clogparser/types.hpp>
#include <clogparser/parser.hpp>
#include <cstdint>
#include <array>
#include <vector>
#include <cassert>
#include <string_view>
#include <numeric>
#include <prescience_helper/ingest.hpp>

namespace prescience_helper::sim {
  struct Target_state;
  struct Player_state;

  using Unit = std::variant<const Target_state*, const Player_state*>;

  struct Damage_amp {
    double crit_amp = 1;
    double crit_chance_add = 0;
  };

  struct Target_state {
    Target_state(std::string_view guid);
    
    std::string_view guid;
    std::string_view name;
    
    //spell_id->stacks
    std::unordered_map<std::uint64_t, std::uint8_t> auras;

    std::uint8_t stacks_of(std::uint64_t spell_id) const noexcept;

    Damage_amp damage_taken;

    void aura_changed(Unit caster, std::uint64_t spell_id, std::uint8_t stacks);
  protected:
    virtual void handle_aura(Unit caster, std::uint64_t spell_id, std::uint8_t new_stacks, std::uint8_t old_stacks);
  };

  //we don't care about haste
  enum class Combat_stat {
    INITIAL = 0,
    mastery_rating = 0,
    mastery_val,
    crit_rating,
    crit_val,
    vers_rating,
    vers_val,
    primary,
    primary_scaling,
    COUNT
  };

  using Combat_stats = clogparser::Enum_indexed_array<Combat_stat, double, Combat_stat::COUNT>;

  struct Calced_damage {
    double base = 0;
    double with_ebon_mult = 1;
    double with_prescience_mult = 1;
    double with_shifting_sands_mult = 1;

    constexpr bool operator==(Calced_damage const&) const noexcept = default;
    constexpr bool operator!=(Calced_damage const&) const noexcept = default;

    constexpr Calced_damage& operator+=(Calced_damage const& o) noexcept {
      std::array<Calced_damage, 2> dmgs{ *this,o };
      *this = combine(dmgs);
      return *this;
    }
    constexpr Calced_damage operator+(Calced_damage const& o) const noexcept {
      Calced_damage me = *this;
      me += o;
      return me;
    }

    constexpr Calced_damage& operator*=(double v) noexcept {
      base *= v;
      return *this;
    }
    constexpr Calced_damage operator*(double v) const noexcept {
      Calced_damage me = *this;
      me *= v;
      return me;
    }

    constexpr Calced_damage& operator/=(double v) noexcept {
      base /= v;
      return *this;
    }
    constexpr Calced_damage operator/(double v) const noexcept {
      Calced_damage me = *this;
      me /= v;
      return me;
    }

    constexpr static Calced_damage combine(std::span<Calced_damage> dmgs) noexcept {
      Calced_damage returning;
      returning.base = std::accumulate(dmgs.begin(), dmgs.end(), 0.0, 
        [](double v1, Calced_damage const& v2) { return v1 + v2.base; });
      returning.with_ebon_mult = std::accumulate(dmgs.begin(), dmgs.end(), 0.0, 
        [](double v1, Calced_damage const& v2) { return v1 + v2.base * v2.with_ebon_mult; }) / returning.base;
      returning.with_prescience_mult = std::accumulate(dmgs.begin(), dmgs.end(), 0.0,
        [](double v1, Calced_damage const& v2) { return v1 + v2.base * v2.with_prescience_mult; }) / returning.base;
      returning.with_shifting_sands_mult = std::accumulate(dmgs.begin(), dmgs.end(), 0.0,
        [](double v1, Calced_damage const& v2) { return v1 + v2.base * v2.with_shifting_sands_mult; }) / returning.base;

      return returning;
    }
  };

  struct Damage {
    double base_scaling = 1;
    bool scales_with_primary = false;
    bool can_not_crit = false;
    bool allow_class_ability_procs = false;
    Damage_amp amp;

    Calced_damage calc(Combat_stats const& damager, Combat_stats const& aug, bool fate_mirror) const noexcept;
  };

  struct Aug_buff {
    const Player_state* aug;
    bool ebon_might = false;
    bool prescience = false;
    bool shifting_sands = false;
  };

  struct Player_state : public Target_state {
  public:
    clogparser::SpecId spec;
    clogparser::Attribute_rating primary_stat;
    Combat_stats initial_stats;
    Combat_stats current_stats;
    std::vector<Aug_buff> aug_buffs;
    std::vector<clogparser::events::Combatant_info::Talent> talents;
    std::vector<clogparser::Item> items;

    Damage_amp damage_done;

    virtual Damage swing(bool crit, Target_state const& target, std::int64_t historical_damage_done) const;
    virtual Damage pet_swing(bool crit, Target_state const& target, std::int64_t historical_damage_done) const;
    virtual Damage impact(std::uint64_t spell_id, bool crit, Target_state const& target, std::int64_t historical_damage_done) const;
    
    static std::unique_ptr<Player_state> create(clogparser::events::Combatant_info const&);

    virtual ~Player_state() {}
  protected:
    virtual void handle_aura(Unit caster, std::uint64_t spell_id, std::uint8_t new_stacks, std::uint8_t old_stacks) override;
    Player_state(clogparser::events::Combatant_info const&, clogparser::Attribute_rating primary_stat);
  };

  constexpr clogparser::events::Combat_log_version::Build_version valid_for{
    10,
    2,
    5
  };
}