#include <prescience_helper/sim/on_rails.hpp>
#include <algorithm>

namespace sim = prescience_helper::sim;

namespace {
  template<typename Parent, typename T>
  struct Event_wrapper {
    Parent* parent;
    T event;
  };

  struct Event {
    clogparser::Period when;
    std::variant<
      Event_wrapper<sim::Target_state, prescience_helper::Aura_changed>,
      Event_wrapper<sim::Player_state, prescience_helper::Spell_impact>,
      Event_wrapper<sim::Player_state, prescience_helper::Swing>,
      Event_wrapper<sim::Player_state, prescience_helper::Pet_swing>> event_variant;
  };

  using Players = std::unordered_map<const prescience_helper::Player*, sim::Player_state*>;
  using Targets = std::unordered_map<const prescience_helper::Target*, prescience_helper::sim::Target_state>;

  const sim::Target_state* convert_to_sim(prescience_helper::Target* caster, Players const& players, Targets const& targets) {
    const auto found = targets.find(caster);
    if (found == targets.end()) {
      throw std::exception("Internal logic error");
    }
    return &found->second;
  }
  sim::Player_state* convert_to_sim(prescience_helper::Player* caster, Players const& players, Targets const& targets) {
    const auto found = players.find(caster);
    if (found == players.end()) {
      throw std::exception("Internal logic error");
    }
    return found->second;
  }

  void do_event(sim::on_rails::Encounter& encounter, clogparser::Period when, Players const& players, Targets const& targets, Event_wrapper<sim::Target_state, prescience_helper::Aura_changed> const& event) {
    sim::Unit caster;
    std::visit([&caster, &players, &targets](auto const& caster_in) {
      caster = convert_to_sim(caster_in, players, targets);
      }, event.event.caster);
    const auto found_generated_player = std::find_if(encounter.players.begin(), encounter.players.end(), [sim_player = event.parent](sim::on_rails::Player const& p) {
      return p.sim_player.get() == sim_player;
    });
    if (found_generated_player != encounter.players.end()) {
      const auto prev_stats = found_generated_player->sim_player->current_stats;
      event.parent->aura_changed(caster, event.event.id, event.event.stacks);
      if (prev_stats != found_generated_player->sim_player->current_stats) {
        found_generated_player->stat_events.push_back(prescience_helper::Event<sim::Combat_stats>{
          when,
          found_generated_player->sim_player->current_stats
        });
      }
    } else {
      event.parent->aura_changed(caster, event.event.id, event.event.stacks);
    }
    
  }
  void do_event(sim::on_rails::Encounter& encounter, clogparser::Period when, Players const& players, Targets const& targets, Event_wrapper<sim::Player_state, prescience_helper::Spell_impact> const& event) {
    const auto found_target = targets.find(event.event.target);
    assert(found_target != targets.end());
    const auto damage = event.parent->impact(event.event.id, event.event.crit, found_target->second, event.event.damage_done);

    const auto found_generated_player = std::find_if(encounter.players.begin(), encounter.players.end(), [sim_player = event.parent](sim::on_rails::Player const& p) {
      return p.sim_player.get() == sim_player;
      });
    assert(found_generated_player != encounter.players.end());
    found_generated_player->damage_events.push_back(prescience_helper::Event<sim::Damage>{
      when,
      damage });
  }
  void do_event(sim::on_rails::Encounter& encounter, clogparser::Period when, Players const& players, Targets const& targets, Event_wrapper<sim::Player_state, prescience_helper::Swing> const& event) {
    const auto found_target = targets.find(event.event.target);
    assert(found_target != targets.end());
    const auto damage = event.parent->swing(event.event.crit, found_target->second, event.event.damage_done);

    const auto found_generated_player = std::find_if(encounter.players.begin(), encounter.players.end(), [sim_player = event.parent](sim::on_rails::Player const& p) {
      return p.sim_player.get() == sim_player;
      });
    assert(found_generated_player != encounter.players.end());
    found_generated_player->damage_events.push_back(prescience_helper::Event<sim::Damage>{
      when,
      damage });
  }
  void do_event(sim::on_rails::Encounter& encounter, clogparser::Period when, Players const& players, Targets const& targets, Event_wrapper<sim::Player_state, prescience_helper::Pet_swing> const& event) {
    const auto found_target = targets.find(event.event.swing.target);
    assert(found_target != targets.end());
    const auto damage = event.parent->pet_swing(event.event.swing.crit, found_target->second, event.event.swing.damage_done);

    const auto found_generated_player = std::find_if(encounter.players.begin(), encounter.players.end(), [sim_player = event.parent](sim::on_rails::Player const& p) {
      return p.sim_player.get() == sim_player;
      });
    assert(found_generated_player != encounter.players.end());
    found_generated_player->damage_events.push_back(prescience_helper::Event<sim::Damage>{
      when,
      damage });
  }

  constexpr sim::Combat_stats ZERO_STATS;
}

sim::on_rails::Encounter sim::on_rails::simulate(prescience_helper::Encounter const& encounter) {

  std::vector<::Event> events;
  Players players;
  Targets targets;
  Encounter generating_encounter;

  generating_encounter.encounter = encounter.start;
  generating_encounter.start_time = encounter.start_time;
  generating_encounter.end_time = encounter.end_time;

  for (auto const& [guid, target] : encounter.targets) {
    const auto emplaced = &targets.emplace(&target, guid).first->second;
    for (auto const& aura_changed : target.aura_changed) {
      events.push_back(::Event{
        aura_changed.when,
        Event_wrapper<sim::Target_state, prescience_helper::Aura_changed>{
          emplaced,
          aura_changed.what } });
    }
  }
  players.reserve(encounter.players.size());
  generating_encounter.players.reserve(encounter.players.size());
  for (auto const& [guid, player] : encounter.players) {
    generating_encounter.players.emplace_back();
    auto& generating_player = generating_encounter.players.back();
    generating_player.ingest_player = &player;
    generating_player.sim_player = sim::Player_state::create(player.info);
    generating_player.stat_events.push_back(Event<Combat_stats>{
      clogparser::Period{ 0 },
      generating_player.sim_player->current_stats
    });
    generating_player.died = player.died;
    generating_player.rezzed = player.rezzed;

    players.emplace(&player, generating_player.sim_player.get());
    const auto added = generating_player.sim_player.get();

    for (auto const& aura_changed : player.aura_changed) {
      events.push_back(::Event{
        aura_changed.when,
        Event_wrapper<sim::Target_state, prescience_helper::Aura_changed>{
          added,
          aura_changed.what } });
    }
    for (auto const& spell_impact : player.spell_impact) {
      events.push_back(::Event{
        spell_impact.when,
        Event_wrapper<sim::Player_state, prescience_helper::Spell_impact>{
          added,
          spell_impact.what } });
    }
    for (auto const& spell_tick : player.spell_tick) {
      events.push_back(::Event{
        spell_tick.when,
        Event_wrapper<sim::Player_state, prescience_helper::Spell_impact>{
          added,
          prescience_helper::Spell_impact{
            spell_tick.what.id,
            spell_tick.what.crit,
            spell_tick.what.damage_done,
            spell_tick.what.target }
      } });
    }
    for (auto const& aura_removed : player.swing) {
      events.push_back(::Event{
        aura_removed.when,
        Event_wrapper<sim::Player_state, prescience_helper::Swing>{
          added,
          aura_removed.what } });
    }
    for (auto const& aura_removed : player.pet_swing) {
      events.push_back(::Event{
        aura_removed.when,
        Event_wrapper<sim::Player_state, prescience_helper::Pet_swing>{
          added,
          aura_removed.what } });
    }
  }

  std::sort(events.begin(), events.end(), [](::Event const& e1, ::Event const& e2) { return e1.when < e2.when; });

  for (auto const& event : events) {
    std::visit([&targets, &players, &generating_encounter, when = event.when](auto const& event) { do_event(generating_encounter, when, players, targets, event); },
      event.event_variant);
  }

  return generating_encounter;
}

std::vector<prescience_helper::Event<sim::Combat_stats>> sim::on_rails::aggregate_stats(
  std::span<const clogparser::Period> duration,
  std::span<const std::vector<Event<sim::Combat_stats>>> stats,
  std::span<const std::vector<Event<void>>> dieds,
  std::span<const std::vector<Event<void>>> rezzeds,
  std::span<const double> weights) noexcept {

  std::vector<Event<Combat_stats>> returning;

  if (duration.size() != stats.size()
    || stats.size() != dieds.size()
    || dieds.size() != rezzeds.size()
    || rezzeds.size() != weights.size()) {
    return returning;
  }

  struct Stats_change {
    Combat_stats to;
    Combat_stats* which;
  };

  struct Died {
    std::size_t i = -1;
  };

  struct Rezzed {
    std::size_t i = -1;
  };

  using Agging_event = Event<std::variant<Stats_change, Died, Rezzed>>;

  std::vector<Agging_event> events;

  std::vector<Combat_stats> agging_stats;
  std::vector<bool> agging_valid;
  agging_stats.resize(stats.size());
  agging_valid.resize(stats.size(), true);

  for (std::size_t i = 0; i < stats.size(); ++i) {
    events.push_back(Agging_event{
      duration[i],
      Died{ i }
      });
    for (auto const& stat : stats[i]) {
      events.push_back(Agging_event{
        stat.when,
        Stats_change{
          stat.what,
          &agging_stats[i]
      } });
    }
    for (auto const& died : dieds[i]) {
      events.push_back(Agging_event{
        died.when,
        Died{ i }
        });
    }
    for (auto const& rezzed : rezzeds[i]) {
      events.push_back(Agging_event{
        rezzed.when,
        Rezzed{ i }
        });
    }
  }

  std::sort(events.begin(), events.end(), [](Agging_event const& e1, Agging_event const& e2) {
    return e1.when < e2.when;
    });

  for (auto iter = events.begin(); iter != events.end();) {
    const auto now = iter->when;
    while (iter != events.end() && iter->when == now) {
      if (std::holds_alternative<Stats_change>(iter->what)) {
        auto const& what = std::get<Stats_change>(iter->what);
        *what.which = what.to;
      } else if (std::holds_alternative<Died>(iter->what)) {
        auto const& what = std::get<Died>(iter->what);
        agging_valid[what.i] = false;
      } else {
        assert(std::holds_alternative<Rezzed>(iter->what));
        auto const& what = std::get<Rezzed>(iter->what);
        agging_valid[what.i] = true;
      }
      ++iter;
    }

    Combat_stats adding;
    double total_weight = 0;
    for (std::size_t i = 0; i < stats.size(); ++i) {
      if (agging_valid[i]) {
        adding += agging_stats[i] * weights[i];
        total_weight += weights[i];
      }
    }
    if (total_weight != 0) { //if total_weight is 0, we've added nothing, this will be replaced with avg when we calc it later
      adding /= total_weight;
    }

    if (!returning.empty()
      && adding == returning.back().what) { //if it's the same, skip adding a new one
      continue;
    }

    returning.push_back({
      now,
      adding
      });
  }

  if (returning.empty()) {
    return returning;
  }

  clogparser::Period total_period{ 0 };
  Combat_stats avg;
  //we insert a new element at the end, to allow for the upcoming for loop to
  //calc the last period of stats and to replace the value with the average
  //
  //if the last element is already zero, we just use that
  if (returning.back().what != ZERO_STATS) {
    returning.push_back({
      *std::max_element(duration.begin(), duration.end()),
      ZERO_STATS
      });
  }

  for (std::size_t i = 1; i < returning.size(); ++i) {
    if (returning[i-1].what != ZERO_STATS) {
      const auto period = returning[i].when - returning[i - 1].when;
      avg += returning[i-1].what * period.count();
      total_period += period;
    }
  }

  avg /= total_period.count();

  for (auto& stat : returning) {
    if (stat.what == ZERO_STATS) {
      stat.what = avg;
    }
  }

  return returning;
}

/*
  IDEA:
  As most people start making non-optimal (long-term) dps choices before they die 
  (either to rot to save themselves, or to boss dying to pop everything they can),
  maybe a good idea would be to ignore any dps events some amount of time before they die
*/
std::vector<prescience_helper::Event<sim::Calced_damage>> sim::on_rails::aggregate_damage(
  std::span<const Event<Combat_stats>> aug,
  std::span<const clogparser::Period> player_duration,
  std::span<const std::vector<Event<Damage>>> player_damage,
  std::span<const std::vector<Event<Combat_stats>>> player_stats,
  std::span<const std::vector<Event<void>>> player_died,
  std::span<const std::vector<Event<void>>> player_rezzed,
  std::span<const double> weights,
  bool fate_mirror,
  clogparser::Period window_size) noexcept {

  std::vector<Event<Calced_damage>> returning;

  if (aug.empty()
    || player_damage.size() != player_stats.size()
    || player_stats.size() != player_died.size()
    || player_died.size() != player_rezzed.size()
    || player_rezzed.size() != weights.size()) {
    return returning;
  }

  const auto size = player_damage.size();

  struct Damage {
    sim::Damage dmg;
    std::size_t i = -1;
  };

  struct Stats_change {
    Combat_stats to;
    std::size_t i = -1;
  };

  struct Aug_stats_change {
    Combat_stats to;
  };

  struct Died {
    std::size_t i = -1;
  };

  struct Rezzed {
    std::size_t i = -1;
  };

  using Agging_event = Event<std::variant<Damage, Stats_change, Died, Rezzed, Aug_stats_change>>;

  std::vector<Agging_event> events;

  for (std::size_t i = 0; i < size; ++i) {
    events.push_back(Agging_event{
      player_duration[i],
      Died{ i }
      });
    for (auto const& damage : player_damage[i]) {
      events.push_back(Agging_event{
        damage.when,
        Damage{ damage.what, i}
        });
    }
    for (auto const& stats : player_stats[i]) {
      events.push_back(Agging_event{
        stats.when,
        Stats_change{ stats.what, i}
        });
    }
    for (auto const& died : player_died[i]) {
      events.push_back(Agging_event{
        died.when,
        Died{i}
        });
    }
    for (auto const& rezzed : player_rezzed[i]) {
      events.push_back(Agging_event{
        rezzed.when,
        Rezzed{i}
        });
    }
  }
  for (auto const& stats : aug) {
    events.push_back(Agging_event{
      stats.when,
      Aug_stats_change{stats.what}
      });
  }

  if (events.empty()) {
    return returning;
  }

  std::sort(events.begin(), events.end(), [](Agging_event const& e1, Agging_event const& e2) {
    return e1.when < e2.when;
    });

  Combat_stats aug_stats{ aug.front().what };
  std::vector<Combat_stats> agging_stats;
  std::vector<double> agging_valid;
  std::vector<bool> agging_alive;

  std::vector<bool> returning_valid;

  for (auto const& stats : player_stats) {
    if (stats.empty()) {
      return returning;
    }
    agging_stats.push_back(stats.front().what);
  }
  agging_valid.resize(player_stats.size(), 1);
  agging_alive.resize(player_stats.size(), true);

  clogparser::Period until = window_size;
  for (auto iter = events.begin(); iter != events.end();) {
    
    Calced_damage calced;
    for (std::size_t i = 0; i < size; ++i) {
      agging_valid[i] = agging_alive[i] ? 1 : 0;
    }

    while (iter != events.end() && iter->when < until) {
      if (std::holds_alternative<Damage>(iter->what)) {
        auto const& what = std::get<Damage>(iter->what);
        if (agging_alive[what.i]) {
          calced += what.dmg.calc(agging_stats[what.i], aug_stats, fate_mirror) * weights[what.i];
        }
      } else if (std::holds_alternative<Stats_change>(iter->what)) {
        auto const& what = std::get<Stats_change>(iter->what);
        agging_stats[what.i] = what.to;
      } else if (std::holds_alternative<Aug_stats_change>(iter->what)) {
        auto const& what = std::get<Aug_stats_change>(iter->what);
        aug_stats = what.to;
      } else if (std::holds_alternative<Died>(iter->what)) {
        auto const& what = std::get<Died>(iter->what);
        if (agging_alive[what.i]) { //need to check this to stop valid going negative as we have a fight end as a death
          agging_alive[what.i] = false;
          agging_valid[what.i] -= ((double)(until - iter->when).count()) / window_size.count();
        }
      } else {
        assert(std::holds_alternative<Rezzed>(iter->what));
        auto const& what = std::get<Rezzed>(iter->what);
        agging_alive[what.i] = true;
        agging_valid[what.i] += ((double)(until - iter->when).count()) / window_size.count();
      }
      ++iter;
    }

    double total_weight = 0;
    for (std::size_t i = 0; i < size; ++i) {
      total_weight += weights[i] * agging_valid[i];
    }

    //normalize to per second amounts. Not really needed
    //total_weight *= (double)std::chrono::duration_cast<clogparser::Period>(std::chrono::seconds{ 1 }).count() / window_size.count();

    if (total_weight != 0) {
      calced /= total_weight;
    }

    bool is_valid = total_weight != 0;

    if (!(!returning.empty()
      && calced == returning.back().what
      && is_valid == returning_valid.back())) { //if NOT same as before

      returning.push_back(Event<Calced_damage>{
        until - window_size,
          calced
      });
      returning_valid.push_back(is_valid);
    }

    until += window_size;
  }

  if (returning.empty()) {
    return returning;
  }

  std::size_t total_windows{ 0 };
  Calced_damage avg;

  //we insert a new element at the end, to allow for the upcoming for loop to
  //calc the last period of damage and to replace the value with the average
  //
  //if the last element is already invalid, we'll just use that
  if (returning_valid.back()) {
    returning.push_back({
      until,
      Calced_damage{}
      });
    returning_valid.push_back(false);
  }

  for (std::size_t i = 1; i < returning.size(); ++i) {
    if (returning_valid[i - 1]) {
      const std::size_t windows = (returning[i].when - returning[i - 1].when) / window_size;
      avg += returning[i - 1].what * windows;
      total_windows += windows;
    }
  }

  avg /= total_windows;

  for (std::size_t i = 0; i < returning.size(); ++i) {
    if (!returning_valid[i]) {
      returning[i].what = avg;
    }
  }

  return returning;
}