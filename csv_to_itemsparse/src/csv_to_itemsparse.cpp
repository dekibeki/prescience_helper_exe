#include <csv_to_include/csv_to_include.hpp>
#include <span>

namespace {
  struct Item_sparse : 
    public csv_to_include::Parser_settings_base {

    static constexpr std::size_t n = 98;
    static constexpr std::array<std::string_view, 23> columns{
      "ID",
      "InventoryType",
      "StatPercentEditor_0",
      "StatPercentEditor_1",
      "StatPercentEditor_2",
      "StatPercentEditor_3",
      "StatPercentEditor_4",
      "StatPercentEditor_5",
      "StatPercentEditor_6",
      "StatPercentEditor_7",
      "StatPercentEditor_8",
      "StatPercentEditor_9",
      "StatModifier_bonusStat_0",
      "StatModifier_bonusStat_1",
      "StatModifier_bonusStat_2",
      "StatModifier_bonusStat_3",
      "StatModifier_bonusStat_4",
      "StatModifier_bonusStat_5",
      "StatModifier_bonusStat_6",
      "StatModifier_bonusStat_7",
      "StatModifier_bonusStat_8",
      "StatModifier_bonusStat_9",
      "ExpansionID"
    };
    
    static constexpr std::string_view UNEQUIPABLE = "0";

    Item_sparse(FILE* header, FILE* source) :
      Parser_settings_base(header, source) {

      fprintf(header_,
        "#pragma once\n"
        "#include <cstdint>\n"
        "#include <unordered_map>\n"
        "#include <array>\n"
        "#include <clogparser/item.hpp>\n"
        "\n"
        "namespace prescience_helper::sim::dbc {\n"
        "  struct Item_sparse {\n"
        "    std::uint64_t id;\n"
        "    struct Stat {\n"
        "      enum class Type : std::int8_t {\n"
        "        unk = -1,\n"
        "        agility = 3,\n"
        "        strength = 4,\n"
        "        intellect = 5,\n"
        "        critical_strike = 32,\n"
        "        haste = 36,\n"
        "        versatility = 40,\n"
        "        mastery = 49,\n"
        "        agility_or_strength_or_intellect = 71,\n"
        "        agility_or_strength = 72,\n"
        "        agility_or_intellect = 73,\n"
        "        strength_or_intellect = 74,\n"
        "      };\n"
        "      Type type;\n"
        "      std::uint16_t amount;\n"
        "\n"
        "      constexpr Stat(int type, int amount) : type(static_cast<Type>(type)), amount(static_cast<std::uint16_t>(amount)) {}\n"
        "    };\n"
        "    std::array<Stat,10> budget;\n"
        "    ::clogparser::Item_slot slot;\n"
        "    \n"
        "    constexpr Item_sparse(std::uint64_t id,\n"
        "      int type0, int amount0, int type1, int amount1,\n"
        "      int type2, int amount2, int type3, int amount3,\n"
        "      int type4, int amount4, int type5, int amount5,\n"
        "      int type6, int amount6, int type7, int amount7,\n"
        "      int type8, int amount8, int type9, int amount9,\n"
        "      int slot) :\n"
        "      id(id), budget{\n"
        "        Stat{type0,amount0},Stat{type1,amount1},Stat{type2,amount2},Stat{type3,amount3},Stat{type4,amount4},\n"
        "        Stat{type5,amount5},Stat{type6,amount6},Stat{type7,amount7},Stat{type8,amount8},Stat{type9,amount9}},\n"
        "      slot(static_cast<::clogparser::Item_slot>(slot)) { }\n"
        "  };\n"
        "\n"
        "  extern const std::unordered_map<std::uint64_t, Item_sparse> ITEM_SPARSE;\n"
        "}\n"
        "\n");

      fprintf(source_,
        "#include <prescience_helper/sim/dbc/item_sparse.hpp>\n"
        "#include <array>\n"
        "namespace dbc = prescience_helper::sim::dbc;\n"
        "\n"
        //"constexpr std::array ITEM_SPARSE_raw{");
        "const std::unordered_map<std::uint64_t, dbc::Item_sparse> dbc::ITEM_SPARSE = []() {\n"
        "  std::unordered_map<std::uint64_t, dbc::Item_sparse> returning;\n");
    }

    void data(std::string_view id, std::string_view slot,
      std::string_view stat_amount0, std::string_view stat_amount1, std::string_view stat_amount2, std::string_view stat_amount3, std::string_view stat_amount4,
      std::string_view stat_amount5, std::string_view stat_amount6, std::string_view stat_amount7, std::string_view stat_amount8, std::string_view stat_amount9,
      std::string_view stat_type0, std::string_view stat_type1, std::string_view stat_type2, std::string_view stat_type3, std::string_view stat_type4,
      std::string_view stat_type5, std::string_view stat_type6, std::string_view stat_type7, std::string_view stat_type8, std::string_view stat_type9,
      std::string_view expansion_id) {

      if (slot == UNEQUIPABLE) {
        return;
      }

      fprintf(source_,
        "    returning.emplace(%.*s, dbc::Item_sparse{ %.*s,"
        " %.*s, %.*s, %.*s, %.*s,"
        " %.*s, %.*s, %.*s, %.*s,"
        " %.*s, %.*s, %.*s, %.*s,"
        " %.*s, %.*s, %.*s, %.*s,"
        " %.*s, %.*s, %.*s, %.*s,"
        " %.*s });\n",
        (std::int32_t)id.size(), id.data(),
        (std::int32_t)id.size(), id.data(),
        (std::int32_t)stat_type0.size(), stat_type0.data(), (std::int32_t)stat_amount0.size(), stat_amount0.data(),
        (std::int32_t)stat_type1.size(), stat_type1.data(), (std::int32_t)stat_amount1.size(), stat_amount1.data(),
        (std::int32_t)stat_type2.size(), stat_type2.data(), (std::int32_t)stat_amount2.size(), stat_amount2.data(),
        (std::int32_t)stat_type3.size(), stat_type3.data(), (std::int32_t)stat_amount3.size(), stat_amount3.data(),
        (std::int32_t)stat_type4.size(), stat_type4.data(), (std::int32_t)stat_amount4.size(), stat_amount4.data(),
        (std::int32_t)stat_type5.size(), stat_type5.data(), (std::int32_t)stat_amount5.size(), stat_amount5.data(),
        (std::int32_t)stat_type6.size(), stat_type6.data(), (std::int32_t)stat_amount6.size(), stat_amount6.data(),
        (std::int32_t)stat_type7.size(), stat_type7.data(), (std::int32_t)stat_amount7.size(), stat_amount7.data(),
        (std::int32_t)stat_type8.size(), stat_type8.data(), (std::int32_t)stat_amount8.size(), stat_amount8.data(),
        (std::int32_t)stat_type9.size(), stat_type9.data(), (std::int32_t)stat_amount9.size(), stat_amount9.data(),
        (std::int32_t)slot.size(), slot.data());
    }

    ~Item_sparse() {
      fprintf(source_,
        "    return returning;\n"
        "  }();\n"
        "\n");
    }
  };
}

int main(int argc, const char** argv) {
  return csv_to_include::main<Item_sparse>(argc, argv);
}