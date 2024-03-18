#pragma once

#include <vector>
#include <string_view>
#include <chrono>
#include <clogparser/parser.hpp>


namespace prescience_helper {
  struct Target;
  struct Player;

  template<typename T>
  struct Event {
    clogparser::Period when;
    T what;
  };

  template<>
  struct Event<void> {
    clogparser::Period when;
  };

  struct Aura_changed {
    std::variant<Target*, Player*> caster;
    std::uint64_t id;
    std::uint8_t stacks;
  };

  struct Spell_impact {
    std::uint64_t id;
    bool crit;
    std::int64_t damage_done;
    Target* target;
  };
  struct Spell_tick {
    std::uint64_t id;
    bool crit;
    std::int64_t damage_done;
    Target* target;
  };

  struct Swing {
    bool crit;
    std::int64_t damage_done;
    Target* target;
  };

  struct Pet_swing {
    std::string_view name;
    Swing swing;
  };
  
  struct Target {
    std::string_view name;
    std::vector<Event<Aura_changed>> aura_changed;
  };

  struct Player : public Target {
    clogparser::events::Combatant_info info;
    std::vector<Event<Spell_impact>> spell_impact;
    std::vector<Event<Spell_tick>> spell_tick;
    std::vector<Event<Swing>> swing;
    std::vector<Event<Pet_swing>> pet_swing;
    std::vector<Event<void>> died;
    std::vector<Event<void>> rezzed;
  };

  struct Encounter {
    Encounter() = default;
    Encounter(Encounter const&) = delete;
    Encounter(Encounter&&) = default;

    Encounter& operator=(Encounter const&) = delete;
    Encounter& operator=(Encounter&&) = default;

    clogparser::Timestamp start_time{ 0 };
    clogparser::events::Encounter_start start;
    std::optional<clogparser::events::Combat_log_version::Build_version> build;
    clogparser::Timestamp end_time{ 0 };
    std::optional<clogparser::events::Encounter_end> end;
    std::unordered_map<std::string_view, Target> targets;
    std::unordered_map<std::string_view, Player> players;
    std::size_t start_byte = 0;
    std::size_t end_byte = 0;
  };

  struct File {
    virtual std::string_view next() = 0;
  };

  void ingest(File& log, clogparser::String_store& strings, std::vector<Encounter>& out);
}