#pragma once
#include <prescience_helper/ingest.hpp>
#include <prescience_helper/sim.hpp>

namespace prescience_helper::sim::on_rails {
  struct Player {
    const prescience_helper::Player* ingest_player = nullptr;
    std::unique_ptr<Player_state> sim_player;
    std::vector<Event<Damage>> damage_events;
    std::vector<Event<Combat_stats>> stat_events;
    std::vector<Event<void>> died;
    std::vector<Event<void>> rezzed;
  };

  struct Encounter {
    clogparser::Timestamp start_time;
    clogparser::Timestamp end_time;
    std::vector<Player> players;
    clogparser::events::Encounter_start encounter;
  };

  template<typename T>
  std::span<T> window(std::span<T> span, std::size_t size) {
    const std::size_t start = span.size() <= size ? 0 : span.size() - size;
    return span.subspan(start);
  }

  Encounter simulate(prescience_helper::Encounter const& encounter);

  std::vector<prescience_helper::Event<Combat_stats>> aggregate_stats(
    std::span<const clogparser::Period> player_duration,
    std::span<const std::vector<Event<Combat_stats>>> stats,
    std::span<const std::vector<Event<void>>> died,
    std::span<const std::vector<Event<void>>> rezzed,
    std::span<const double> weights) noexcept;

  std::vector<prescience_helper::Event<Calced_damage>> aggregate_damage(
    std::span<const Event<Combat_stats>> aug,
    std::span<const clogparser::Period> player_duration,
    std::span<const std::vector<Event<Damage>>> player_damage,
    std::span<const std::vector<Event<Combat_stats>>> player_stats,
    std::span<const std::vector<Event<void>>> player_died,
    std::span<const std::vector<Event<void>>> player_rezzed,
    std::span<const double> weights,
    bool fate_mirror,
    clogparser::Period window_size = std::chrono::seconds{ 1 }) noexcept;
}