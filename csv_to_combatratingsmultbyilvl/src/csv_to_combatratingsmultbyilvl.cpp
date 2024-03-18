#include <csv_to_include/csv_to_include.hpp>
#include <span>

namespace {

  struct Combat_rating_mult_by_ilvl : csv_to_include::Parser_settings_base {
    static constexpr char column_delim = '\t';
    static constexpr std::size_t n = 5;
    static constexpr std::array<std::string_view, 5> columns{
      "Item Level",
      "Armor Multiplier",
      "Weapon Multiplier",
      "Trinket Multiplier",
      "Jewelry Multiplier"
    };

    Combat_rating_mult_by_ilvl(FILE* header, FILE* source) : Parser_settings_base(header, source) {

      fprintf(header_,
        "#pragma once\n"
        "#include <cstdint>\n"
        "#include <unordered_map>\n"
        "\n"
        "namespace prescience_helper::sim::dbc {\n"
        "  struct Combat_ratings_mult_by_ilvl {\n"
        "    std::uint16_t ilvl;\n"
        "    float armor;\n"
        "    float weapon;\n"
        "    float trinket;\n"
        "    float jewelry;\n"
        "\n"
        "    constexpr Combat_ratings_mult_by_ilvl(std::uint16_t ilvl, float armor, float weapon, float trinket, float jewelry) :\n"
        "      ilvl(ilvl), armor(armor), weapon(weapon), trinket(trinket), jewelry(jewelry) { }\n"
        "  };\n"
        "\n"
        "  extern const std::unordered_map<std::uint16_t, Combat_ratings_mult_by_ilvl> COMBAT_RATINGS_MULT_BY_ILVL;\n"
        "}\n"
        "\n");

      fprintf(source_,
        "#include <prescience_helper/sim/dbc/combat_ratings_mult_by_ilvl.hpp>\n"
        "namespace dbc = prescience_helper::sim::dbc;\n"
        "\n"
        "const std::unordered_map<std::uint16_t, dbc::Combat_ratings_mult_by_ilvl> dbc::COMBAT_RATINGS_MULT_BY_ILVL = []() {\n"
        "  std::unordered_map<std::uint16_t, dbc::Combat_ratings_mult_by_ilvl> returning;\n");
    }

    void data(std::string_view ilvl, std::string_view armor, std::string_view weapon, std::string_view trinket, std::string_view jewelry) {

      fprintf(source_,
        "  returning.emplace(%.*s, Combat_ratings_mult_by_ilvl{ %.*s,"
        " %.*s, %.*s, %.*s, %.*s });\n",
        (std::int32_t)ilvl.size(), ilvl.data(),
        (std::int32_t)ilvl.size(), ilvl.data(),
        (std::int32_t)armor.size(), armor.data(),
        (std::int32_t)weapon.size(), weapon.data(),
        (std::int32_t)trinket.size(), trinket.data(),
        (std::int32_t)jewelry.size(), jewelry.data());
    }

    ~Combat_rating_mult_by_ilvl() {
      fprintf(source_,
        "  return returning;\n"
        "}();\n"
        "\n");
    }
  };
}

int main(int argc, const char** argv) {
  return csv_to_include::main<Combat_rating_mult_by_ilvl>(argc, argv);
}