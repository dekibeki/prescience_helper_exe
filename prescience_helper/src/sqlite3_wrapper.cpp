#include <prescience_helper/sqlite3_wrapper.hpp>

void prescience_helper::internal::bind_val(sqlite3_stmt* stmt, int i, std::string_view val) {
  sqlite3_bind_text(stmt, i, val.data(), val.size(), SQLITE_TRANSIENT);
}

void prescience_helper::internal::bind_val(sqlite3_stmt* stmt, int i, std::int32_t val) {
  sqlite3_bind_int(stmt, i, val);
}

void prescience_helper::internal::bind_val(sqlite3_stmt* stmt, int i, std::int64_t val) {
  sqlite3_bind_int64(stmt, i, val);
}

void prescience_helper::internal::bind_val(sqlite3_stmt* stmt, int i, std::span<const std::byte> val) {
  sqlite3_bind_blob(stmt, i, val.data(), (int)val.size(), SQLITE_TRANSIENT);
}

std::int64_t prescience_helper::internal::column(sqlite3_stmt* from, int i, Tag<std::int64_t>) {
  return sqlite3_column_int64(from, i);
}

std::string_view prescience_helper::internal::column(sqlite3_stmt* from, int i, Tag<std::string_view>) {
  return std::string_view{ reinterpret_cast<const char*>(sqlite3_column_text(from, i)), (std::size_t)sqlite3_column_bytes(from, i) };
}

std::int32_t prescience_helper::internal::column(sqlite3_stmt* from, int i, Tag<std::int32_t>) {
  return sqlite3_column_int(from, i);
}

std::span<const std::byte> prescience_helper::internal::column(sqlite3_stmt* from, int i, Tag<std::span<const std::byte>>) {
  return std::span<const std::byte>{static_cast<const std::byte*>(sqlite3_column_blob(from, i)), (std::size_t)sqlite3_column_bytes(from, i)};
}

prescience_helper::Stmt::Stmt(sqlite3_stmt* stmt) :
  stmt_(stmt) {

}

bool prescience_helper::Stmt::valid() const noexcept {
  return stmt_ != nullptr;
}

void prescience_helper::Stmt::Sqlite3_stmt_closer_::operator()(sqlite3_stmt* stmt) {
  sqlite3_finalize(stmt);
}

prescience_helper::Db::Db(sqlite3* db) :
  db_(db) {

}

bool prescience_helper::Db::valid() const noexcept {
  return db_ != nullptr;
}

void prescience_helper::Db::begin() const noexcept {
  sqlite3_exec(db_.get(), "BEGIN;", nullptr, nullptr, nullptr);
}

void prescience_helper::Db::rollback() const noexcept {
  sqlite3_exec(db_.get(), "ROLLBACK;", nullptr, nullptr, nullptr);
}

void prescience_helper::Db::commit() const noexcept {
  sqlite3_exec(db_.get(), "COMMIT;", nullptr, nullptr, nullptr);
}

std::int64_t prescience_helper::Db::last_insert_rowid() const noexcept {
  return sqlite3_last_insert_rowid(db_.get());
}

prescience_helper::Stmt prescience_helper::Db::prepare(std::string_view query) const {
  sqlite3_stmt* stmt = nullptr;

  int v;

  if ((v = sqlite3_prepare_v2(db_.get(), query.data(), query.size(), &stmt, nullptr)) != SQLITE_OK) {
    return Stmt{ nullptr };
  } else {
    return Stmt{ stmt };
  }
}

void prescience_helper::Db::Sqlite3_closer_::operator()(sqlite3* db) {
  sqlite3_close_v2(db);
}

prescience_helper::Settings::Settings(Db const& db):
  update_stmt_(db.prepare("UPDATE Configs SET value=? WHERE name=?;")) {

  db.exec<std::string_view, std::string_view>("SELECT name,value FROM Configs;", [this](std::string_view name, std::string_view value) {
    configs_[std::string{ name }] = std::string{ value };
    });
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
  update_stmt_.exec([]() {}, found->second, name);
}