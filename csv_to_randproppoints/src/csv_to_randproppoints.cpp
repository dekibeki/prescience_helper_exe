#include <csv_to_include/csv_to_include.hpp>
#include <span>
#include <vector>

namespace {
  struct Rand_prop_points : public csv_to_include::Parser_settings_base {
    static constexpr std::size_t n = 35;
    static constexpr std::array<std::string_view, 18> columns{
      "ID",
      "DamageReplaceStatF",
      "DamageSecondaryF",
      "EpicF_0",
      "EpicF_1",
      "EpicF_2",
      "EpicF_3",
      "EpicF_4",
      "SuperiorF_0",
      "SuperiorF_1",
      "SuperiorF_2",
      "SuperiorF_3",
      "SuperiorF_4",
      "GoodF_0",
      "GoodF_1",
      "GoodF_2",
      "GoodF_3",
      "GoodF_4"
    };

    Rand_prop_points(FILE* header, FILE* source) :
      Parser_settings_base(header, source) {

      fprintf(header_,
        "#pragma once\n"
        "#include <cstdint>\n"
        "#include <array>\n"
        "#include <unordered_map>"
        "\n"
        "namespace prescience_helper::sim::dbc {\n"
        "  struct Rand_prop_point {\n"
        "    std::uint32_t ilvl;\n"
        "    float damage_replace_stat;\n"
        "    float damage_secondary;\n"
        "    std::array<float,5> budget;\n"
        "  };\n"
        "\n"
        "  extern const std::unordered_map<std::uint32_t,Rand_prop_point> RAND_PROP_POINT;\n"
        "}\n"
        "\n");

      fprintf(source_,
        "#include <prescience_helper/sim/dbc/rand_prop_points.hpp>\n"
        "namespace dbc = prescience_helper::sim::dbc;\n"
        "\n"
        "const std::unordered_map<std::uint32_t, dbc::Rand_prop_point> dbc::RAND_PROP_POINT = []() {\n"
        "  std::unordered_map<std::uint32_t, dbc::Rand_prop_point> returning;\n");
    }
    void data(std::string_view id, std::string_view damage_replace_stat, std::string_view damage_secondary,
      std::string_view epic0, std::string_view epic1, std::string_view epic2, std::string_view epic3, std::string_view epic4,
      std::string_view superior0, std::string_view superior1, std::string_view superior2, std::string_view superior3, std::string_view superior4,
      std::string_view good0, std::string_view good1, std::string_view good2, std::string_view good3, std::string_view good4) {
      if (epic0 != superior0 || superior0 != good0) {
        throw std::exception{ "Stat weights vary by item type" };
      }
      if (epic1 != superior1 || superior1 != good1) {
        throw std::exception{ "Stat weights vary by item type" };
      }
      if (epic2 != superior2 || superior2 != good2) {
        throw std::exception{ "Stat weights vary by item type" };
      }
      if (epic3 != superior3 || superior3 != good3) {
        throw std::exception{ "Stat weights vary by item type" };
      }
      if (epic4 != superior4 || superior4 != good4) {
        throw std::exception{ "Stat weights vary by item type" };
      }

      fprintf(source_,
        "  returning.emplace(%.*s, Rand_prop_point{ %.*s, %.*s, %.*s, { %.*s, %.*s, %.*s, %.*s, %.*s } });\n",
        (std::int32_t)id.size(), id.data(),
        (std::int32_t)id.size(), id.data(),
        (std::int32_t)damage_replace_stat.size(), damage_replace_stat.data(),
        (std::int32_t)damage_secondary.size(), damage_secondary.data(),
        (std::int32_t)epic0.size(), epic0.data(),
        (std::int32_t)epic1.size(), epic1.data(),
        (std::int32_t)epic2.size(), epic2.data(),
        (std::int32_t)epic3.size(), epic3.data(),
        (std::int32_t)epic4.size(), epic4.data());
    }
    ~Rand_prop_points() {
      fprintf(source_,
        "  return returning;\n"
        "}();\n"
        "\n");
    }
  };
}

int main(int argc, const char** argv) {
  return csv_to_include::main<Rand_prop_points>(argc, argv);
}