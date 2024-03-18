#pragma once
#include <cstdint>
#include <unordered_map>
#include <array>
#include <clogparser/item.hpp>

namespace prescience_helper::sim::dbc {
  struct Item_sparse {
    std::uint64_t id;
    struct Stat {
      enum class Type : std::int8_t {
        unk = -1,
        agility = 3,
        strength = 4,
        intellect = 5,
        critical_strike = 32,
        haste = 36,
        versatility = 40,
        mastery = 49,
        agility_or_strength_or_intellect = 71,
        agility_or_strength = 72,
        agility_or_intellect = 73,
        strength_or_intellect = 74,
      };
      Type type;
      std::uint16_t amount;

      constexpr Stat(int type, int amount) : type(static_cast<Type>(type)), amount(static_cast<std::uint16_t>(amount)) {}
    };
    std::array<Stat,10> budget;
    ::clogparser::Item_slot slot;
    
    constexpr Item_sparse(std::uint64_t id,
      int type0, int amount0, int type1, int amount1,
      int type2, int amount2, int type3, int amount3,
      int type4, int amount4, int type5, int amount5,
      int type6, int amount6, int type7, int amount7,
      int type8, int amount8, int type9, int amount9,
      int slot) :
      id(id), budget{
        Stat{type0,amount0},Stat{type1,amount1},Stat{type2,amount2},Stat{type3,amount3},Stat{type4,amount4},
        Stat{type5,amount5},Stat{type6,amount6},Stat{type7,amount7},Stat{type8,amount8},Stat{type9,amount9}},
      slot(static_cast<::clogparser::Item_slot>(slot)) { }
  };

  extern const std::unordered_map<std::uint64_t, Item_sparse> ITEM_SPARSE;
}

