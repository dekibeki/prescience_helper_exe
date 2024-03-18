#include <filesystem>
#include <unordered_map>
#include <optional>

namespace prescience_helper {
  struct Log_finder {
  public:
    struct Log {
      std::filesystem::path path;
      std::uintmax_t old_useful;
      std::uintmax_t old_total;
      std::uintmax_t new_total;
      std::optional<std::int32_t> last_patch;
    };

    Log_finder(std::filesystem::path);

    void set_log_read(std::filesystem::path, std::uintmax_t useful_amount, std::uintmax_t total_amount, std::optional<std::int32_t> last_patch);

    void change_path(std::filesystem::path);

    std::filesystem::path const& path() const noexcept {
      return base_;
    }

    void check(std::vector<Log>& returning);
    std::vector<Log> check();
  private:
    struct Cache_entry {
      std::uintmax_t useful;
      std::uintmax_t total;
      std::optional<std::int32_t> last_patch;
    };

    std::filesystem::path base_;
    std::unordered_map<std::filesystem::path, Cache_entry> cache_;
  };
}