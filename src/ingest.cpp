#include <prescience_helper/ingest.hpp>
#include <clogparser/parser.hpp>
#include <cassert>
#include <cstdint>

namespace events = clogparser::events;

namespace {
  struct State {
    State(clogparser::String_store& strings) :
      strings(strings) {

    }
    std::vector<prescience_helper::Encounter> encounters;
    clogparser::String_store& strings;
    bool in_encounter = false;
    std::unordered_map<std::string_view, std::vector<std::int64_t>> unknown_player_damages;
    std::unordered_map<std::string_view, std::string_view> owners;

    std::optional<events::Combat_log_version::Build_version> build_version;

    std::int64_t& get_damage(std::string_view guid, clogparser::Timestamp event_timestamp) {
      auto& encounter = encounters.back();
      const auto found = encounter.players.find(guid);
      std::vector<std::int64_t>* damage_array = nullptr;
      if (found == encounter.players.end()) {
        damage_array = &unknown_player_damages[strings.get(guid)];
      } else {
        damage_array = &found->second.damage;
      }

      const std::chrono::seconds delta = std::chrono::duration_cast<std::chrono::seconds>(event_timestamp - encounter.start_time);

      assert(delta.count() >= 0);

      if (damage_array->size() <= delta.count()) {
        damage_array->resize(delta.count() + 1, 0);
      }
      return (*damage_array)[delta.count()];
    }

    void handle_advanced_info(events::Advanced_info const& advanced) {
      owners[strings.get(advanced.advanced_unit_guid)] = strings.get(advanced.owner_guid);
    }

    void end_encounter(clogparser::Timestamp when, std::optional<events::Encounter_end> end) {
      if (!in_encounter) {
        return;
      }
      auto& encounter = encounters.back();
      if (end) {
        encounter.end = strings.get(*end);
      }
      encounter.end_time = when;
      in_encounter = false;

      for (auto const& [guid, damages] : unknown_player_damages) {
        const auto found_owner_guid = owners.find(guid);
        if (found_owner_guid == owners.end()) {
          continue;
        }

        const auto found_owner = encounter.players.find(found_owner_guid->second);
        if (found_owner == encounter.players.end()) {
          continue;
        }

        if (damages.size() > found_owner->second.damage.size()) {
          found_owner->second.damage.resize(damages.size(), 0);
        }

        for (std::size_t i = 0; i < damages.size(); ++i) {
          found_owner->second.damage[i] += damages[i];
        }
      }
    }

    void operator()(clogparser::Timestamp when, events::Combat_log_version const& version) {
      build_version = version.build_version;
      end_encounter(when, std::nullopt);
    }
    void operator()(clogparser::Timestamp when, events::Encounter_start const& event) {

      //if 5 players of less, we don't care
      if (event.instance_size <= 5) {
        return;
      }

      in_encounter = true;
      encounters.emplace_back();
      encounters.back().start = strings.get(event);
      encounters.back().start_time = when;
      encounters.back().build = build_version;

      unknown_player_damages.clear();
      owners.clear();
    }
    void operator()(clogparser::Timestamp when, events::Combatant_info const& event) {
      if (!in_encounter) {
        return;
      }
      events::Combatant_info new_info = strings.get(event);
      encounters.back().players[new_info.guid].info = new_info;
    }
    void operator()(clogparser::Timestamp when, events::Encounter_end const& event) {
      end_encounter(when, event);
    }
    void operator()(clogparser::Timestamp when, events::Spell_periodic_damage const& event) {
      if (!in_encounter
        || !event.combat_header.source.flags.is(clogparser::Unit_flags::Unit_type::player)
        || event.combat_header.dest.flags.is(clogparser::Unit_flags::Unit_type::player)) {
        return;
      }
      handle_advanced_info(event.advanced);
      get_damage(event.combat_header.source.guid, when) += event.damage.final;    
    }
    void operator()(clogparser::Timestamp when, events::Spell_periodic_damage_support const& event) {
      if (!in_encounter) {
        return;
      }
      handle_advanced_info(event.advanced);
      get_damage(event.combat_header.source.guid, when) -= event.damage.final;
    }
    void operator()(clogparser::Timestamp when, events::Spell_damage const& event) {
      if (!in_encounter
        || !event.combat_header.source.flags.is(clogparser::Unit_flags::Unit_type::player)
        || event.combat_header.dest.flags.is(clogparser::Unit_flags::Unit_type::player)) {
        return;
      }
      handle_advanced_info(event.advanced);
      get_damage(event.combat_header.source.guid, when) += event.damage.final;
    }
    void operator()(clogparser::Timestamp when, events::Spell_damage_support const& event) {
      if (!in_encounter) {
        return;
      }
      handle_advanced_info(event.advanced);
      get_damage(event.combat_header.source.guid, when) -= event.damage.final;
    }
    void operator()(clogparser::Timestamp when, events::Spell_cast_success const& event) {
      if (!in_encounter) {
        return;
      }

      handle_advanced_info(event.advanced);
    }
    void operator()(clogparser::Timestamp when, events::Spell_summon const& event) {
      if (!in_encounter) {
        return;
      }
      owners[strings.get(event.summoned.guid)] = strings.get(event.summoner.guid);
    }
    void operator()(clogparser::Timestamp when, events::Swing_damage const& event) {
      if (!in_encounter) {
        return;
      }

      handle_advanced_info(event.advanced);
    }
    void operator()(clogparser::Timestamp when, events::Swing_damage_landed const& event) {
      if (!in_encounter
        || !event.combat_header.source.flags.is(clogparser::Unit_flags::Unit_type::player)
        || event.combat_header.dest.flags.is(clogparser::Unit_flags::Unit_type::player)) {
        return;
      }
      handle_advanced_info(event.advanced);
      get_damage(event.combat_header.source.guid, when) += event.damage.final;
    }
    void operator()(clogparser::Timestamp when, events::Swing_damage_landed_support const& event) {
      if (!in_encounter) {
        return;
      }
      handle_advanced_info(event.advanced);
      get_damage(event.combat_header.source.guid, when) -= event.damage.final;
    }
    void operator()(clogparser::Timestamp when, events::Zone_change const& event) {
      end_encounter(when, std::nullopt);
    }
  };
}

std::vector<prescience_helper::Encounter> prescience_helper::ingest(File& log, clogparser::String_store& strings) {
  State state{ strings };

  clogparser::Parser<State&> parser{ state };

  for (std::string_view recved = log.next(); !recved.empty(); recved = log.next()) {
    parser.parse(recved);
  }

  if (state.in_encounter) {
    state.encounters.pop_back();
  }

  return state.encounters;
}