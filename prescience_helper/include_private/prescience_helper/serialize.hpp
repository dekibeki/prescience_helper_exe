#pragma once

#include <span>
#include <vector>
#include <cstddef>
#include <cassert>
#include <cstdint>
#include <limits>
#include <bit>
#include <sstream>

static_assert(std::numeric_limits<double>::is_iec559);
static_assert(std::numeric_limits<float>::is_iec559);

namespace prescience_helper::serialize {
  struct Write_buffer {
  public:
    constexpr Write_buffer(std::vector<std::byte>& into) :
      underlying_(into) {
    }

    void reserve_more(std::size_t n) {
      underlying_.reserve(underlying_.size() + n);
    }

    template<typename T>
    void write(T val) {
      underlying_.resize(underlying_.size() + sizeof(T));
      write_impl_<T>(val);
    }

    template<typename U, typename T>
    void write_clamped(T val) {
      const U clamped = std::min<T>(val, std::numeric_limits<U>::max());
      this->write(clamped);
    }

    template<typename First, typename ...Rest>
    void write(First first, Rest... rest) {
      write(first);
      if constexpr (sizeof...(Rest) > 0) {
        write(rest...);
      }
    }

    std::vector<std::byte> finish() noexcept {
      std::vector<std::byte> returning = std::move(underlying_);
      underlying_.clear();
      return returning;
    }
  private:
    template<typename T>
    constexpr void write_impl_(T val) {
      if constexpr (std::is_same_v<T, float>) {
        write_impl_(std::bit_cast<std::uint32_t>(val));
      } else if constexpr (std::is_same_v<T, double>) {
        write_impl_(std::bit_cast<std::uint64_t>(val));
      } else if constexpr (std::is_signed_v<T>) {
        write_impl_(std::bit_cast<std::make_unsigned_t<T>>(val));
      } else if constexpr (std::is_same_v<T, bool>) {
        write_impl_<std::uint8_t>(val ? 1 : 0);
      } else {
        static_assert(
          std::is_integral_v<T> 
          && std::is_unsigned_v<T>);

        std::span<std::byte> writing_into = std::span{ underlying_ }.subspan(underlying_.size() - sizeof(T));

        for (std::size_t i = 0; i < sizeof(T); ++i) {
          writing_into[i] = static_cast<std::byte>((val >> (8 * i)) & 0xFF);
        }
      }
    }
    
    std::vector<std::byte>& underlying_;
  };

  struct Read_buffer {
  public:
    constexpr Read_buffer(std::span<const std::byte> underlying) :
      underlying_(underlying) {

    }

    constexpr bool empty() const {
      return underlying_.empty();
    }

    template<typename T>
    constexpr T read() {
      assert(underlying_.size() >= sizeof(T));
      const auto returning = read_impl_<T>();
      underlying_ = underlying_.subspan(sizeof(T));
      return returning;
    }
  private:
    template<typename T>
    constexpr T read_impl_() {
      if constexpr (std::is_same_v<T, float>) {
        return std::bit_cast<float>(read_impl_<std::uint32_t>());
      } else if constexpr (std::is_same_v<T, double>) {
        return std::bit_cast<double>(read_impl_<std::uint64_t>());
      } else if constexpr (std::is_signed_v<T>) {
        return std::bit_cast<T>(read_impl_<std::make_unsigned_t<T>>());
      } else if constexpr (std::is_same_v<T, bool>) {
        return read_impl_<std::uint8_t>() != 0;
      } else {
        static_assert(
          std::is_integral_v<T>
          && std::is_unsigned_v<T>);

        T returning{ 0 };

        for (std::size_t i = 0; i < sizeof(T); ++i) {
          returning |= static_cast<T>(underlying_[i]) << (8 * i);
        }

        return returning;
      }
    }
    std::span<const std::byte> underlying_;
  };

  void to_ascii_85(std::stringstream& out, std::span<const std::byte> in);
}