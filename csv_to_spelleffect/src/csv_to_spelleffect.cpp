#include <csv_to_include/csv_to_include.hpp>
#include <span>

namespace {

  static constexpr std::string_view SCHOOL_DAMAGE = "2";

  struct Spell_effect : csv_to_include::Parser_settings_base {
    static constexpr std::size_t n = 36;
    static constexpr std::array<std::string_view, 5> columns{
      "SpellID",
      "Effect",
      "EffectBonusCoefficient",
      "BonusCoefficientFromAP",
      "DifficultyID"
    };

    Spell_effect(FILE* header, FILE* source) : Parser_settings_base(header, source) {

      fprintf(header_,
        "#pragma once\n"
        "#include <cstdint>\n"
        "\n"
        "namespace prescience_helper::sim::dbc {\n"
        "  bool scales_with_primary(std::uint64_t) noexcept;\n"
        "}\n"
        "\n");

      fprintf(source_,
        "#include <prescience_helper/sim/dbc/spell_effect.hpp>\n"
        "#include <unordered_set>\n"
        "namespace dbc = prescience_helper::sim::dbc;\n"
        "\n"
        "namespace {\n"
        "  struct Spell_effect {\n"
        "    std::unordered_set<std::uint64_t> scales_with_primary;\n"
        "\n"
        "    Spell_effect();\n"
        "  } const SPELL_EFFECT;\n"
        "\n"
        "  __declspec(noinline) void emplace(std::unordered_set<std::uint64_t>& into, std::uint64_t id) {\n"
        "    into.insert(id);\n"
        "  }\n"
        "}\n"
        "bool dbc::scales_with_primary(std::uint64_t spell_id) noexcept {\n"
        "  return SPELL_EFFECT.scales_with_primary.contains(spell_id);\n"
        "}\n"
        "\n"
        "Spell_effect::Spell_effect() {\n");
    }

    void data(std::string_view spell_id, std::string_view effect, std::string_view coefficient, std::string_view ap_coefficient, std::string_view difficulty_id) {

      if (effect != SCHOOL_DAMAGE || (coefficient == "0"  && ap_coefficient == "0") || difficulty_id != "0") {
        return;
      }

      fprintf(source_,
        "  ::emplace(this->scales_with_primary, %.*s);\n",
        (std::int32_t)spell_id.size(), spell_id.data());
    }

    ~Spell_effect() {
      fprintf(source_,
        "}\n"
        "\n");
    }
  };
}

int main(int argc, const char** argv) {
  return csv_to_include::main<Spell_effect>(argc, argv);
}