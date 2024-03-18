#include <prescience_helper/sim/helpers.hpp>
#include <prescience_helper/sim/dbc_constants.hpp>

namespace sim = prescience_helper::sim;


std::uint16_t sim::ilvl_of_item(Player_state const& player, std::uint64_t item_id) {
  for (auto const& item : player.items) {
    if (item.item_id == item_id) {
      return item.ilvl;
    }
  }
  return 0;
}

double sim::calc_mastery(Combat_stats const& stats) {
  return stats[Combat_stat::mastery_val] + dbc_constants::scale_rating(stats[Combat_stat::mastery_rating] / dbc_constants::rating_per_mastery);
}

double sim::calc_crit(Combat_stats const& stats) {
  return stats[Combat_stat::crit_val] + dbc_constants::scale_rating(stats[Combat_stat::crit_rating] / dbc_constants::rating_per_crit) / 100;
}

double sim::calc_vers(Combat_stats const& stats) {
  return stats[Combat_stat::vers_val] + dbc_constants::scale_rating(stats[Combat_stat::vers_rating] / dbc_constants::rating_per_vers_damage) / 100;
}