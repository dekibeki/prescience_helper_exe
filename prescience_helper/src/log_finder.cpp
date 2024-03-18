#include <prescience_helper/log_finder.hpp>
#include <cassert>

prescience_helper::Log_finder::Log_finder(std::filesystem::path base) :
  base_(std::move(base)) {

  assert(std::filesystem::is_directory(base_));
}

void prescience_helper::Log_finder::change_path(std::filesystem::path path) {
  base_ = path;
}

void prescience_helper::Log_finder::set_log_read(std::filesystem::path path, std::uintmax_t useful_amount, std::uintmax_t total_amount, std::optional<std::int32_t> last_patch) {
  auto& editing = cache_[path];
  editing.useful = useful_amount;
  editing.total = total_amount;
  editing.last_patch = std::move(last_patch);
}

void prescience_helper::Log_finder::check(std::vector<Log>& returning) {
  if (!std::filesystem::exists(base_) || !std::filesystem::is_directory(base_)) {
    return;
  }
  try {
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
        cache_.emplace(dir_entry.path(), Cache_entry{ 0,file_size });
        returning.push_back({
          dir_entry.path(),
          0,
          0,
          file_size });
      } else {
        if (found->second.total < file_size) {
          returning.push_back({
            found->first,
            found->second.useful,
            found->second.total,
            file_size,
            found->second.last_patch 
            });
          found->second.total = file_size;
        }
      }
    }
  }
  catch (...) {
    return;
  }
}

std::vector<prescience_helper::Log_finder::Log> prescience_helper::Log_finder::check() {
  std::vector<Log> returning;
  check(returning);
  return returning;
}