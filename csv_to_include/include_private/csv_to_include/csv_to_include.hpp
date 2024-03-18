#pragma once
#include <array>
#include <string_view>
#include <cassert>
#include <cstdio>
#include <vector>
#include <optional>
#include <span>

namespace csv_to_include {
  constexpr std::size_t BUFFER_SIZE = 64 * 1024 * 1024; //64mb

  template<typename Settings, std::size_t ...i>
  void forward_to_data(Settings& to, std::span<std::string_view, Settings::n> columns, std::span<std::size_t, Settings::columns.size()> indexes, std::index_sequence<i...>) {
    to.data(columns[indexes[i]]...);
  }

  struct Parsed {
    std::string_view rest;
    bool found = false;
    std::string_view found_str;
  };

  struct Parser_base {
  public:
    template<char delim, char... quotes>
    Parsed parse_for(std::string_view in) {
      std::string_view::size_type start = 0;

      std::string_view::size_type found_char;

      std::array<char, 1+sizeof...(quotes)> search_string = {quotes..., delim};

      Parsed returning;

      for (;;) {
        if (looking_for_quote_) {
          found_char = in.find(*looking_for_quote_, start);
        } else {
          found_char = in.find_first_of(std::string_view{ search_string.data(), search_string.size() }, start);
        }

        if (found_char == std::string_view::npos) {
          return returning;
        } else if (in[found_char] != delim) { //isn't delim, must be a quote char
          if (looking_for_quote_) {
            looking_for_quote_.reset();
          } else {
            looking_for_quote_ = in[found_char];
          }
          start = found_char + 1;
        } else { //is delim
          assert(in[found_char] == delim);
          assert(!looking_for_quote_);

          returning.found_str = in.substr(0, found_char);
          returning.rest = in.substr(found_char + 1);
          returning.found = true;

          return returning;
        }
      }
    }
  private:
    std::optional<char> looking_for_quote_;
  };

  struct Parser_settings_base {
    Parser_settings_base(FILE* header, FILE* source) :
      header_(header), source_(source) {}
    static constexpr char line_delim = '\n';
    static constexpr char column_delim = ',';

    FILE* header_ = nullptr;
    FILE* source_ = nullptr;
  };

  template<typename Settings>
  struct Parser : public Settings {
  private:
    static constexpr std::size_t column_n_ = Settings::n;
    static_assert(column_n_ > 0);
    static constexpr auto interesting_columns = Settings::columns;
    static constexpr std::size_t interesting_columns_n = interesting_columns.size();

    static constexpr char using_line_delim_ = Settings::line_delim;
    static constexpr char using_column_delim_ = Settings::column_delim;
  public:
    using Settings::Settings;

    void read(std::string_view in) {
      Parsed res;

      while ((res = base_.parse_for<using_line_delim_,'"'>(in)).found) {
        if (saved_.empty()) {
          parse_line_(res.found_str);
        } else {
          saved_.append(res.found_str);
          parse_line_(saved_);
          saved_.clear();
        }
        in = res.rest;
      }
      saved_.append(in);
    }
  private:
    void parse_line_(std::string_view in) {
      std::array<std::string_view, column_n_> columns;

      Parsed res;
      std::size_t in_i = 0;
      while ((res = base_.parse_for<using_column_delim_, '"'>(in)).found) {
        if (column_n_ <= in_i) {
          throw std::exception{ "Too many columns in row to generate an output row" };
        }
        columns[in_i] = res.found_str;
        ++in_i;
        in = res.rest;
      }
      if (column_n_ <= in_i) {
        throw std::exception{ "Too many columns in row to generate an output row" };
      }
      columns[in_i] = in;
      ++in_i;

      if (in_i != column_n_) {
        throw std::exception{ "Not enough columns in row to generate output row" };
      }

      if (parsed_header_) {
        forward_to_data(*this, columns, interesting_columns_indexes_, std::make_index_sequence<interesting_columns_n>{});
      } else {
        parse_header_(columns);
        parsed_header_ = true;
      }
    }
    void parse_header_(std::span<std::string_view, column_n_> columns) {
      for (std::size_t i = 0; i < interesting_columns_n; ++i) {
        const auto found = std::find(columns.begin(), columns.end(), interesting_columns[i]);
        if (found == columns.end()) {
          fprintf(stderr, "Couldn't find column %.*s in header\n",
            (std::int32_t)interesting_columns[i].size(), interesting_columns[i].data());
          std::exit(-5);
        }

        interesting_columns_indexes_[i] = std::distance(columns.begin(), found);
      }
    }
    std::array<std::size_t, interesting_columns_n> interesting_columns_indexes_ = {};
    bool parsed_header_ = false;
    Parser_base base_;
    std::string saved_;
  };

  template<typename Settings>
  int main(int argc, const char** argv) {
    if (argc < 4) {
      fprintf(stderr, "Expected %s <input csv path> <output header path> <output source path>\n", argv[0]);
      return -1;
    }

    const char* const in_path = argv[1];
    const char* const header_out_path = argv[2];
    const char* const source_out_path = argv[3];

    FILE* in = fopen(in_path, "r");
    if (in == nullptr) {
      fprintf(stderr, "Couldn't open input at '%s'\n", in_path);
      return -2;
    }
    FILE* header_out = fopen(header_out_path, "w");
    if (header_out == nullptr) {
      fprintf(stderr, "Couldn't open output at '%s'\n", header_out_path);
      return -3;
    }
    FILE* source_out = fopen(source_out_path, "w");
    if (source_out == nullptr) {
      fprintf(stderr, "Couldn't open output at '%s'\n", source_out_path);
      return -4;
    }

    Parser<Settings> impl{ header_out, source_out };

    std::vector<char> buffer;
    buffer.resize(BUFFER_SIZE);

    std::size_t read = 0;

    while ((read = fread(buffer.data(), sizeof(char), BUFFER_SIZE, in)) > 0) {
      impl.read(std::string_view{ buffer.data(), read });
    }

    return 0;
  }
}