#include <csv_to_include/csv_to_include.hpp>
#include <clogparser/parser.hpp>
#include <span>
#include <bit>

namespace {
  constexpr std::uint64_t CAN_NOT_CRIT = 0x20000000;
  constexpr std::uint64_t ALLOW_CLASS_ABILITY_PROCS = 0x1;

  constexpr std::string_view true_str = "true";
  constexpr std::string_view false_str = "false";

  struct Spell_misc : csv_to_include::Parser_settings_base {
    static constexpr std::size_t n = 31;
    static constexpr std::array<std::string_view, 3> columns{
      "SpellID",
      "Attributes_2",
      "Attributes_13"
    };

    Spell_misc(FILE* header, FILE* source) : Parser_settings_base(header, source) {

      fprintf(header_,
        "#pragma once\n"
        "#include <cstdint>\n"
        "\n"
        "namespace prescience_helper::sim::dbc {\n"
        "  bool can_not_crit(std::uint64_t) noexcept;\n"
        "  bool allow_class_ability_procs(std::uint64_t) noexcept;\n"
        "}\n"
        "\n");

      fprintf(source_,
        "#include <prescience_helper/sim/dbc/spell_misc.hpp>\n"
        "#include <unordered_set>\n"
        "\n"
        "namespace dbc = prescience_helper::sim::dbc;\n"
        "\n"
        "namespace {\n"
        "  struct Spell_misc {\n"
        "    std::unordered_set<std::uint64_t> can_not_crit;\n"
        "    std::unordered_set<std::uint64_t> allow_class_ability_procs;\n"
        "\n"
        "    Spell_misc();\n"
        "  } const SPELL_MISC;\n"
        "\n"
        "  __declspec(noinline) void emplace(std::unordered_set<std::uint64_t>& into, std::uint64_t id) {\n"
        "    into.insert(id);\n"
        "  }\n"
        "}\n"
        "\n"
        "bool dbc::can_not_crit(std::uint64_t spell_id) noexcept {\n"
        "  return SPELL_MISC.can_not_crit.contains(spell_id);\n"
        "}\n"
        "\n"
        "bool dbc::allow_class_ability_procs(std::uint64_t spell_id) noexcept {\n"
        "  return SPELL_MISC.allow_class_ability_procs.contains(spell_id);\n"
        "}\n"
        "\n"
        "Spell_misc::Spell_misc() {\n");
    }

    void data(std::string_view spell_id, std::string_view attr2_str, std::string_view attr13_str) {
      const std::uint64_t attr2 = std::bit_cast<std::uint64_t>(clogparser::helpers::parseInt<std::int64_t>(attr2_str));
      const bool can_not_crit = (attr2 & CAN_NOT_CRIT) != 0;

      if (can_not_crit) {
        fprintf(source_,
          "  ::emplace(this->can_not_crit, %.*s);\n",
          (std::int32_t)spell_id.size(), spell_id.data());
      }

      const std::uint64_t attr13 = std::bit_cast<std::uint64_t>(clogparser::helpers::parseInt<std::int64_t>(attr13_str));
      const bool allow_class_ability_procs = (attr13 & ALLOW_CLASS_ABILITY_PROCS) != 0;

      if (allow_class_ability_procs) {
        fprintf(source_,
          "  ::emplace(this->allow_class_ability_procs, %.*s);\n",
          (std::int32_t)spell_id.size(), spell_id.data());
      }
    }

    ~Spell_misc() {
      fprintf(source_,
        "}\n"
        "\n");
    }
  };
}

int main(int argc, const char** argv) {
  return csv_to_include::main<Spell_misc>(argc, argv);
}