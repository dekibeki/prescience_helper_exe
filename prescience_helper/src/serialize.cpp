#include <prescience_helper/serialize.hpp>
#include <optional>
#include <array>

namespace {
  void write_quad(std::stringstream& out, std::uint32_t in) {
    if (in == 0) {
      out << 'z';
    } else {
      for (std::size_t j = 0; j < 5; ++j) {
        const auto rem = in % 85;
        in /= 85;

        out << std::bit_cast<char>(static_cast<std::uint8_t>(rem + 32));
      }
    }
  }
}

void prescience_helper::serialize::to_ascii_85(std::stringstream& out, std::span<const std::byte> in) {
  const auto quad_count = in.size() / 4;
  std::optional<std::array<std::byte,4>> last_quad;
  if (const auto rem = in.size() % 4; rem != 0) {
    std::array<std::byte, 4> constructing;
    for (std::size_t i = 0; i < rem; ++i) {
      constructing[i] = in[in.size() - rem + i];
    }
    for (std::size_t i = rem; i < 4; ++i) {
      constructing[i] = std::byte{ 0 };
    }
    last_quad = constructing;
  }

  Read_buffer reader{ in };

  for (std::size_t i = 0; i < quad_count; ++i) {
    const std::uint32_t writing = reader.read<std::uint32_t>();
    write_quad(out, writing);
  }

  if (last_quad) {
    Read_buffer last_quad_reader{ *last_quad };
    const std::uint32_t writing = last_quad_reader.read<std::uint32_t>();
    write_quad(out, writing);
  }
}