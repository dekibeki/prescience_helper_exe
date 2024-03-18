#include <sqlite3.h>

#include <memory>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <span>
#include <vector>
#include <thread>
#include <cstddef>

namespace prescience_helper {
  
  namespace internal {
    void bind_val(sqlite3_stmt* stmt, int i, std::string_view val);

    void bind_val(sqlite3_stmt* stmt, int i, std::int32_t val);

    void bind_val(sqlite3_stmt* stmt, int i, std::int64_t val);

    void bind_val(sqlite3_stmt* stmt, int i, std::span<const std::byte> val);

    template<typename T>
    void bind_val(sqlite3_stmt* stmt, int i, std::optional<T> val) {
      if (val) {
        bind_val(stmt, i, *val);
      } else {
        sqlite3_bind_null(stmt, i);
      }
    }

    constexpr void bind_loop(sqlite3_stmt* stmt, int i) {}

    template<typename First, typename ...Rest>
    void bind_loop(sqlite3_stmt* stmt, int i, First&& first, Rest&&... rest) {
      bind_val(stmt, i, std::forward<First>(first));
      bind_loop(stmt, i + 1, std::forward<Rest>(rest)...);
    }

    template<typename T>
    struct Tag {};

    std::int64_t column(sqlite3_stmt* from, int i, Tag<std::int64_t>);

    std::string_view column(sqlite3_stmt* from, int i, Tag<std::string_view>);

    std::int32_t column(sqlite3_stmt* from, int i, Tag<std::int32_t>);

    std::span<const std::byte> column(sqlite3_stmt* from, int i, Tag<std::span<const std::byte>>);

    template<typename T>
    std::optional<T> column(sqlite3_stmt* from, int i, Tag<std::optional<T>>) {
      if (sqlite3_column_type(from, i) == SQLITE_NULL) {
        return std::nullopt;
      } else {
        return column(from, i, Tag<T>{});
      }
    }

    template<typename ...T, std::size_t ...vals, typename Cb>
    void get_columns_and_call_cb(sqlite3_stmt* from, Cb&& cb, std::index_sequence<vals...>) {
      cb(column(from, vals, Tag<T>{})...);
    }
  }

  struct Db;

  struct Stmt {
  public:
    Stmt(sqlite3_stmt*);

    bool valid() const noexcept;

    template<typename ...Out, typename ...In, typename Func>
    void execEx(sqlite3* db, Func&& cb, In&&... in) const requires std::is_invocable_v<Func, Out...> {
      sqlite3_reset(stmt_.get());
      internal::bind_loop(stmt_.get(), 1, std::forward<In>(in)...);
      int v;
      for (;;) {
        switch (v = sqlite3_step(stmt_.get())) {
        case SQLITE_BUSY:
          std::this_thread::sleep_for(std::chrono::seconds::zero());
          break;
        case SQLITE_DONE:
          return;
        case SQLITE_ROW:
          internal::get_columns_and_call_cb<Out...>(stmt_.get(), std::forward<Func>(cb), std::index_sequence_for<Out...>{});
          break;
        default:
          if (db) {
            const auto errormsg = sqlite3_errmsg(db);
            fprintf(stderr, "Error execing sql: %s\n", errormsg);
          }
          std::terminate();
        }
      }
    }
    template<typename ...Out, typename ...In, typename Func>
    void exec(Func&& cb, In&&... in) const requires std::is_invocable_v<Func, Out...> {
      this->execEx<Out...>(nullptr, std::forward<Func>(cb), std::forward<In>(in)...);
    }
  private:
    struct Sqlite3_stmt_closer_ {
      void operator()(sqlite3_stmt* stmt);
    };

    std::unique_ptr<sqlite3_stmt, Sqlite3_stmt_closer_> stmt_;
  };

  struct Db {
  public:
    Db(sqlite3*);

    bool valid() const noexcept;

    void begin() const noexcept;
    void rollback() const noexcept;
    void commit() const noexcept;
    std::int64_t last_insert_rowid() const noexcept;
    template<typename ...Out, typename ...In, typename Func>
    void exec(std::string_view query, Func&& cb, In&&... in) const requires std::is_invocable_v<Func, Out...> {
      Stmt created = prepare(query);

      created.exec<Out...>(std::forward<Func>(cb), std::forward<In>(in)...);
    }

    Stmt prepare(std::string_view query) const;
  private:
    struct Sqlite3_closer_ {
    void operator()(sqlite3 * db);
    };
    std::unique_ptr<sqlite3, Sqlite3_closer_> db_;
  };

  struct Settings {
  public:
    Settings(Db const& db);

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