#include <sqlite3.h>

#include <memory>
#include <string_view>
#include <unordered_map>

namespace prescience_helper {
  struct Sqlite3_stmt_closer {
    void operator()(sqlite3_stmt* stmt) {
      sqlite3_finalize(stmt);
    }
  };

  using Stmt = std::unique_ptr<sqlite3_stmt, Sqlite3_stmt_closer>;

  Stmt prepare(sqlite3* db, std::string_view query);

  void bind_val(Stmt const& stmt, int i, std::string_view val);

  void bind_val(Stmt const& stmt, int i, std::int32_t val);

  void bind_val(Stmt const& stmt, int i, std::int64_t val);

  constexpr void bind_loop(Stmt const& stmt, int i) { }

  template<typename First, typename ...Rest>
  void bind_loop(Stmt const& stmt, int i, First&& first, Rest&&... rest) {
    bind_val(stmt, i, std::forward<First>(first));
    bind_loop(stmt, i + 1, std::forward<Rest>(rest)...);
  }

  template<typename ...T>
  void bind_sql(Stmt const& stmt, T&&... args) {
    sqlite3_reset(stmt.get());
    bind_loop(stmt, 1, std::forward<T>(args)...);
  }

  template<typename T>
  struct Tag { };

  std::int64_t column(Stmt const& from, int i, Tag<std::int64_t>);

  std::string_view column(Stmt const& from, int i, Tag<std::string_view>);

  std::int32_t column(Stmt const& from, int i, Tag<std::int32_t>);

  template<typename ...T, std::size_t ...vals, typename Cb>
  void get_columns_and_call_cb(Stmt const& from, Cb&& cb, std::index_sequence<vals...>) {
    cb(column(from, vals, Tag<T>{})...);
  }

  template<typename ...T, typename Func>
  void exec(Stmt const& stmt, Func&& cb) {
    int v;
    for (;;) {
      switch (v = sqlite3_step(stmt.get())) {
      case SQLITE_BUSY:
        break;;
      case SQLITE_DONE:
        return;
      case SQLITE_ROW:
      {
        get_columns_and_call_cb<T...>(stmt, std::forward<Func>(cb), std::index_sequence_for<T...>{});
        break;
      }
      default:
        std::terminate();
      }
    }
  }

  struct Sqlite3_closer {
    void operator()(sqlite3* db);
  };

  using Db = std::unique_ptr<sqlite3, Sqlite3_closer>;

  struct Settings {
  public:
    Settings(sqlite3* db);

    std::string_view get(std::string_view name) const noexcept;

    void set(std::string_view name, std::string value) noexcept;

  private:
    Stmt update_stmt_;
    struct String_hash {
      using is_transparent = void;
      std::size_t operator()(std::string_view in) const noexcept {
        return std::hash<std::string_view>{}(in);
      }
      std::size_t operator()(std::string const& in) const noexcept {
        return std::hash<std::string_view>{}(in);
      }
    };
    std::unordered_map<std::string, std::string, String_hash, std::equal_to<void>> configs_;
  };
}