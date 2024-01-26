#include <prescience_helper/log_finder.hpp>
#include <cassert>

prescience_helper::Log_finder::Log_finder(std::filesystem::path base) :
  base_(std::move(base)) {

  assert(std::filesystem::is_directory(base_));
}

void prescience_helper::Log_finder::change_path(std::filesystem::path path) {
  base_ = path;
}

void prescience_helper::Log_finder::set_log_read(std::filesystem::path path, std::uintmax_t from) {
  cache_[path] = from;
}

void prescience_helper::Log_finder::check(std::vector<Log>& returning) {
  for (auto const& dir_entry : std::filesystem::directory_iterator{ base_ }) {

    if (!dir_entry.is_regular_file() || !dir_entry.path().has_filename()) {
      continue;
    }

    if (!dir_entry.path().filename().string().starts_with("WoWCombatLog-")) {
      continue;
    }

    const auto file_size = dir_entry.file_size();
    const auto found = cache_.find(dir_entry.path());

    if (found == cache_.end()) {
      cache_.emplace(dir_entry.path(), file_size);
      returning.push_back({
        dir_entry.path(),
        0,
        file_size});
    } else {
      if (found->second < file_size) {
        returning.push_back({
          found->first,
          found->second,
          file_size});
        found->second = file_size;
      }
    }
  }
}

std::vector<prescience_helper::Log_finder::Log> prescience_helper::Log_finder::check() {
  std::vector<Log> returning;
  check(returning);
  return returning;
}