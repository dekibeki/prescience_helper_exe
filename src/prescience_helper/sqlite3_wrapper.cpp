#include <prescience_helper/sqlite3_wrapper.hpp>

prescience_helper::Stmt prescience_helper::prepare(sqlite3* db, std::string_view query) {
  sqlite3_stmt* stmt = nullptr;

  int v;

  if ((v = sqlite3_prepare_v2(db, query.data(), query.size(), &stmt, nullptr)) != SQLITE_OK) {
    return nullptr;
  } else {
    return Stmt{ stmt };
  }
}

void prescience_helper::bind_val(Stmt const& stmt, int i, std::string_view val) {
  sqlite3_bind_text(stmt.get(), i, val.data(), val.size(), SQLITE_STATIC);
}

void prescience_helper::bind_val(Stmt const& stmt, int i, std::int32_t val) {
  sqlite3_bind_int(stmt.get(), i, val);
}

void prescience_helper::bind_val(Stmt const& stmt, int i, std::int64_t val) {
  sqlite3_bind_int64(stmt.get(), i, val);
}

std::int64_t prescience_helper::column(Stmt const& from, int i, Tag<std::int64_t>) {
  return sqlite3_column_int64(from.get(), i);
}

std::string_view prescience_helper::column(Stmt const& from, int i, Tag<std::string_view>) {
  return std::string_view{ reinterpret_cast<const char*>(sqlite3_column_text(from.get(), i)), (std::size_t)sqlite3_column_bytes(from.get(), i) };
}

std::int32_t prescience_helper::column(Stmt const& from, int i, Tag<std::int32_t>) {
  return sqlite3_column_int(from.get(), i);
}

void prescience_helper::Sqlite3_closer::operator()(sqlite3* db) {
  sqlite3_close_v2(db);
}

prescience_helper::Settings::Settings(sqlite3* db) {
  const auto get_stmt = prepare(db, "SELECT name,value FROM Configs;");
  exec<std::string_view, std::string_view>(get_stmt, [this](std::string_view name, std::string_view value) {
    configs_[std::string{ name }] = std::string{ value };
    });

  update_stmt_ = prepare(db, "UPDATE Configs SET value=? WHERE name=?;");
}

std::string_view prescience_helper::Settings::get(std::string_view name) const noexcept {
  const auto found = configs_.find(name);
  if (found == configs_.end()) {
    return "";
  } else {
    return found->second;
  }
}

void prescience_helper::Settings::set(std::string_view name, std::string value) noexcept {
  const auto found = configs_.find(name);
  if (found == configs_.end()) {
    return;
  }
  found->second = std::move(value);
  bind_sql(update_stmt_, found->second, name);
  exec(update_stmt_, []() {});
}