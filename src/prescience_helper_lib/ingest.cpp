#include <prescience_helper/ingest.hpp>
#include <prescience_helper_lib/sim.hpp>
#include <prescience_helper_lib/spells.hpp>
#include <clogparser/parser.hpp>
#include <cassert>
#include <cstdint>
#include <array>
#include <sstream>
#include <algorithm>
#include <unordered_map>

namespace events = clogparser::events;

namespace {

  struct Friendly  {
    std::string_view name;
    std::vector<prescience_helper::Event<prescience_helper::Spell_impact>> spell_impact;
    std::vector<prescience_helper::Event<prescience_helper::Spell_tick>> spell_tick;
    std::vector<prescience_helper::Event<prescience_helper::Swing>> swing;
    prescience_helper::Player* owner = nullptr;
  };

  template<typename T>
  void append(std::vector<T>& to, std::vector<T> const& from) {
    to.insert(to.end(), from.begin(), from.end());
  }

  template<typename T>
  void sort_event_vector(std::vector<prescience_helper::Event<T>>& vec) {
    std::sort(vec.begin(), vec.end(), [](prescience_helper::Event<T> const& e1, prescience_helper::Event<T> const& e2) {
      return e1.when < e2.when;
      });
  }

  struct State {
    State(clogparser::String_store& strings) :
      strings(strings) {

    }
    std::vector<prescience_helper::Encounter> encounters;
    std::unordered_map<std::string_view, Friendly> friendlies;
    std::unordered_map<std::string_view, std::string_view> guid_to_names;
    clogparser::String_store& strings;
    bool in_encounter = false;
    std::optional<events::Combat_log_version::Build_version> build_version;

    prescience_helper::Target* get_target(std::string_view guid) {
      assert(!encounters.empty());

      auto& encounter = encounters.back();

      if (const auto found = encounter.players.find(guid); found != encounter.players.end()) {
        return &found->second;
      } else if (const auto found = encounter.targets.find(guid); found != encounter.targets.end()) {
        return &found->second;
      } else {
        return &encounter.targets[strings.get(guid)];
      }
    }
    Friendly* get_friendly(std::string_view guid) {
      assert(!encounters.empty());

      if (const auto found = friendlies.find(guid); found != friendlies.end()) {
        return &found->second;
      } else {
        return &friendlies[strings.get(guid)];
      }
    }

    void handle(events::Advanced_info const& advanced) {
      if (!in_encounter) {
        return;
      }

      auto& encounter = encounters.back();

      if (const auto found = encounter.players.find(advanced.owner_guid); found != encounter.players.end()) {
        auto& friendly = friendlies[strings.get(advanced.advanced_unit_guid)];
        assert(friendly.owner == nullptr || friendly.owner == &found->second);
        friendly.owner = &found->second;
      }
    }
    void handle(events::Unit const& unit) {
      if (!in_encounter) {
        return;
      }

      if (const auto found = guid_to_names.find(unit.guid); found == guid_to_names.end()) {
        guid_to_names[strings.get(unit.guid)] = strings.get(unit.name);
      } else {
        assert(found->second == unit.name);
      }
    }
    void handle(events::Combat_header const& combat_header) {
      handle(combat_header.source);
      handle(combat_header.dest);
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

      std::size_t unk_name_count = 1;

      const auto set_name = [this, &unk_name_count](auto& container) {
        for (auto& [guid, unit] : container) {
          if (const auto found_name = guid_to_names.find(guid); found_name != guid_to_names.end()) {
            unit.name = found_name->second;
          } else {
            std::stringstream name_constructor;
            name_constructor << "Unknown" << unk_name_count;
            ++unk_name_count;
            unit.name = strings.get(name_constructor.view());
          }
        }};

      set_name(encounter.targets);
      set_name(friendlies);
      set_name(encounter.players);

      for (auto const& [guid, friendly] : friendlies) {
        if (friendly.owner != nullptr) {
          append(friendly.owner->spell_impact, friendly.spell_impact);
          append(friendly.owner->spell_tick, friendly.spell_tick);
          friendly.owner->pet_swing.reserve(friendly.owner->pet_swing.size() + friendly.swing.size());
          for (auto const& e : friendly.swing) {
            friendly.owner->pet_swing.push_back({
              e.when,
              prescience_helper::Pet_swing{
                friendly.name,
                e.what}
              });
          }
        }
      }
      for (auto& [guid,player] : encounter.players) {
        sort_event_vector(player.spell_impact);
        sort_event_vector(player.spell_tick);
        sort_event_vector(player.pet_swing);
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
      handle(event.advanced);
      handle(event.combat_header);

      auto& encounter = encounters.back();

      const prescience_helper::Event<prescience_helper::Spell_tick> adding{
          when - encounter.start_time,
          prescience_helper::Spell_tick{
            event.spell.id,
            event.damage.crit,
            event.damage.final,
            get_target(event.combat_header.dest.guid) }
      };

      if (const auto found = encounter.players.find(event.combat_header.source.guid); found != encounter.players.end()) {
        found->second.spell_tick.push_back(adding);
      } else {
        get_friendly(event.combat_header.source.guid)->spell_tick.push_back(adding);
      }
    }
    void operator()(clogparser::Timestamp when, events::Spell_damage const& event) {
      if (!in_encounter
        || !event.combat_header.source.flags.is(clogparser::Unit_flags::Unit_type::player)
        || event.combat_header.dest.flags.is(clogparser::Unit_flags::Unit_type::player)) {
        return;
      }
      handle(event.advanced);
      handle(event.combat_header);

      auto& encounter = encounters.back();

      const prescience_helper::Event<prescience_helper::Spell_impact> adding{
          when - encounter.start_time,
          prescience_helper::Spell_impact{
            event.spell.id,
            event.damage.crit,
            event.damage.final,
            get_target(event.combat_header.dest.guid) }
      };

      if (const auto found = encounter.players.find(event.combat_header.source.guid); found != encounter.players.end()) {
        found->second.spell_impact.push_back(adding);
      } else {
        get_friendly(event.combat_header.source.guid)->spell_impact.push_back(adding);
      }
    }
    void operator()(clogparser::Timestamp when, events::Spell_aura_applied const& event) {
      if (!in_encounter) {
        return;
      }

      auto& encounter = encounters.back();

      const prescience_helper::Event<prescience_helper::Aura_added> adding{
        when - encounter.start_time,
        prescience_helper::Aura_added{
          event.spell.id,
          event.remaining_points}
      };

      if (event.combat_header.dest.flags.is(clogparser::Unit_flags::Unit_type::player)) {
        if (const auto found = encounter.players.find(event.combat_header.dest.guid); found != encounter.players.end()) {
          found->second.aura_added.push_back(adding);
        }
      } else {
        get_target(event.combat_header.dest.guid)->aura_added.push_back(adding);
      }
    }
    void operator()(clogparser::Timestamp when, events::Spell_aura_removed const& event) {
      if (!in_encounter) {
        return;
      }

      auto& encounter = encounters.back();

      const prescience_helper::Event<prescience_helper::Aura_removed> adding{
        when - encounter.start_time,
        prescience_helper::Aura_removed{
          event.spell.id}
      };

      if (event.combat_header.dest.flags.is(clogparser::Unit_flags::Unit_type::player)) {
        if (const auto found = encounter.players.find(event.combat_header.dest.guid); found != encounter.players.end()) {
          found->second.aura_removed.push_back(adding);
        }
      } else {
        get_target(event.combat_header.dest.guid)->aura_removed.push_back(adding);
      }
    }
    void operator()(clogparser::Timestamp when, events::Spell_cast_success const& event) {
      if (!in_encounter) {
        return;
      }

      handle(event.advanced);
      handle(event.combat_header);
    }
    void operator()(clogparser::Timestamp when, events::Spell_summon const& event) {
      if (!in_encounter) {
        return;
      }

      auto& encounter = encounters.back();

      if (const auto found = encounter.players.find(event.summoner.guid); found != encounter.players.end()) {
        auto& friendly = friendlies[strings.get(event.summoned.guid)];
        assert(friendly.owner == nullptr || friendly.owner == &found->second);
        friendly.owner = &found->second;
      }
    }
    void operator()(clogparser::Timestamp when, events::Swing_damage const& event) {
      if (!in_encounter) {
        return;
      }

      handle(event.advanced);
      handle(event.combat_header);

      if (!in_encounter
        || !event.combat_header.source.flags.is(clogparser::Unit_flags::Unit_type::player)
        || event.combat_header.dest.flags.is(clogparser::Unit_flags::Unit_type::player)) {
        return;
      }
      handle(event.advanced);
      handle(event.combat_header);

      auto& encounter = encounters.back();

      const prescience_helper::Event<prescience_helper::Swing> adding{
          when - encounter.start_time,
          prescience_helper::Swing{
            event.damage.crit,
            event.damage.final,
            get_target(event.combat_header.dest.guid) }
      };

      if (const auto found = encounter.players.find(event.combat_header.source.guid); found != encounter.players.end()) {
        found->second.swing.push_back(adding);
      } else {
        get_friendly(event.combat_header.source.guid)->swing.push_back(adding);
      }
    }
    void operator()(clogparser::Timestamp when, events::Swing_damage_landed const& event) {
      if (!in_encounter
        || !event.combat_header.source.flags.is(clogparser::Unit_flags::Unit_type::player)
        || event.combat_header.dest.flags.is(clogparser::Unit_flags::Unit_type::player)) {
        return;
      }
      handle(event.advanced);
      handle(event.combat_header);
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