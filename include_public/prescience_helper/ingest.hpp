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
    std::chrono::milliseconds when;
    T what;
  };

  struct Aura_added {
    std::uint64_t id;
    std::optional<std::uint64_t> remaining_points;
  };

  struct Aura_removed {
    std::uint64_t id;
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
    std::vector<Event<Aura_added>> aura_added;
    std::vector<Event<Aura_removed>> aura_removed;
  };

  struct Player : public Target {
    clogparser::events::Combatant_info info;
    std::vector<Event<Spell_impact>> spell_impact;
    std::vector<Event<Spell_tick>> spell_tick;
    std::vector<Event<Swing>> swing;
    std::vector<Event<Pet_swing>> pet_swing;
  };

  struct Encounter {
    clogparser::Timestamp start_time{ 0 };
    clogparser::events::Encounter_start start;
    std::optional<clogparser::events::Combat_log_version::Build_version> build;
    clogparser::Timestamp end_time{ 0 };
    std::optional<clogparser::events::Encounter_end> end;
    std::unordered_map<std::string_view, Target> targets;
    std::unordered_map<std::string_view, Player> players;
  };

  struct File {
    virtual std::string_view next() = 0;
  };

  std::vector<Encounter> ingest(File& log, clogparser::String_store& strings);
}