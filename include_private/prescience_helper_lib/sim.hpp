#pragma once

#include <clogparser/types.hpp>
#include <clogparser/parser.hpp>
#include <cstdint>
#include <array>
#include <vector>
#include <cassert>
#include <string_view>

namespace prescience_helper_lib::internal {
  struct Target_state;
  struct Player_state;

  struct Spell_type {
    std::uint64_t id;
    std::string_view name;
    void (*cast)(Player_state const& caster, Target_state const& castee);
  };

  struct Aura_type {
    std::uint64_t id;
    std::string_view name;
  };

  struct Dummy_aura {
    Aura_type type;
  };

  template<typename T>
  void undordered_remove_from_vector(std::vector<T>& vec, std::size_t i) {
    assert(!vec.empty());
    assert(i < vec.size());

    if (i == vec.size() - 1) {
      vec.pop_back();
    } else {
      vec[i] = std::move(vec.back());
      vec.pop_back();
    }
  }

  struct Damage_amp {
    double flat_increase = 0;
    double normal_amp = 1;
    double crit_amp = 1;
    double crit_change_add = 0;
  };

  struct Target_state {
    Target_state(std::string_view guid);
    
    std::string_view guid;
    
    std::vector<Dummy_aura> dummy_auras;
    Damage_amp phys_damage_taken;
    Damage_amp magic_damage_taken;

    virtual void aura_applied(std::uint64_t spell_id, std::optional<std::uint64_t> remaining_points);
    virtual void aura_removed(std::uint64_t spell_id);
  };

  enum class Combat_stat {
    INITIAL = 0,
    spell_power = 0,
    attack_power,
    mastery,
    crit,
    vers,
    primary,
    COUNT
  };

  constexpr Combat_stat& operator++(Combat_stat& v) noexcept {
    using Underlying_type = std::underlying_type_t<Combat_stat>;
    v = static_cast<Combat_stat>(static_cast<Underlying_type>(v) + 1);
    return v;
  }

  using Combat_stats = clogparser::Enum_indexed_array<Combat_stat, double, Combat_stat::COUNT>;

  struct Player_scaling {
    clogparser::Attribute_rating primary;
    double sp_per_primary = 0;
    double ap_per_primary = 0;
    double sp_per_ap = 0;
    double ap_per_sp = 0;
    double primary_scaling = 1;
    double sp_scaling = 1;
    double ap_scaling = 1;

    void add_primary(Combat_stats& to, double amount) const noexcept;
    void add_sp_scaling(Combat_stats& to, double amount) noexcept;
    void add_ap_scaling(Combat_stats& to, double amount) noexcept;
  };

  enum class Buffs : std::uint8_t {
    none = 0,
    ebon_might = (1 << 0),
    prescience = (1 << 1),
    shifting_sands = (1 << 2),
    COMBINATIONS = (1 << 3)
  };

  constexpr Buffs operator|(Buffs b1, Buffs b2) noexcept {
    using Underlying_type = std::underlying_type_t<Buffs>;
    return static_cast<Buffs>(static_cast<Underlying_type>(b1) | static_cast<Underlying_type>(b2));
  }
  constexpr Buffs operator&(Buffs b1, Buffs b2) noexcept {
    using Underlying_type = std::underlying_type_t<Buffs>;
    return static_cast<Buffs>(static_cast<Underlying_type>(b1) & static_cast<Underlying_type>(b2));
  }
  constexpr Buffs& operator++(Buffs& b1) noexcept {
    using Underlying_type = std::underlying_type_t<Buffs>;
    b1 = static_cast<Buffs>(static_cast<Underlying_type>(b1) + 1);
    return b1;
  }

  using Spell_result = clogparser::Enum_indexed_array<Buffs, double, Buffs::COMBINATIONS>;

  struct Player_state : public Target_state {
  public:
    clogparser::SpecId spec;
    Player_scaling scaling;

    Combat_stats current_stats;
    std::vector<clogparser::events::Combatant_info::Talent> talents;

    std::vector<Dummy_aura> dummy_auras;
    Damage_amp physical_damage_done;
    Damage_amp magic_damage_done;

    virtual Spell_result impact(std::uint64_t spell_id, bool crit, Player_state const& aug, Target_state const& target) const = 0;
    virtual Spell_result tick(std::uint64_t spell_id, bool crit, Player_state const& aug, Target_state const& target) const = 0;
    virtual void aura_applied(std::uint64_t spell_id, std::optional<std::uint64_t> remaining_points) override;
    virtual void aura_removed(std::uint64_t spell_id) override;

    static std::unique_ptr<Player_state> create(clogparser::events::Combatant_info const&);

    virtual ~Player_state() {}
  protected:
    Player_state(clogparser::events::Combatant_info const&, Player_scaling const& player_scaling);
  };
}