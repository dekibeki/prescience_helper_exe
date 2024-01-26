#pragma once

#include <vector>
#include <string_view>
#include <clogparser/parser.hpp>

namespace prescience_helper {
  struct Player {
    std::vector<std::int64_t> damage;
    clogparser::events::Combatant_info info;
  };

  struct Encounter {
    clogparser::Timestamp start_time{ 0 };
    clogparser::events::Encounter_start start;
    std::optional<clogparser::events::Combat_log_version::Build_version> build;
    clogparser::Timestamp end_time{ 0 };
    std::optional<clogparser::events::Encounter_end> end;
    std::unordered_map<std::string_view, Player> players;
  };

  struct File {
    virtual std::string_view next() = 0;
  };

  std::vector<Encounter> ingest(File& log, clogparser::String_store& strings);
}