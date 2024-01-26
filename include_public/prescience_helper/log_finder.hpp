#include <filesystem>
#include <unordered_map>

namespace prescience_helper {
  struct Log_finder {
  public:
    struct Log {
      std::filesystem::path path;
      std::uintmax_t from;
      std::uintmax_t to;
    };

    Log_finder(std::filesystem::path);

    void set_log_read(std::filesystem::path, std::uintmax_t from);

    void change_path(std::filesystem::path);

    void check(std::vector<Log>& returning);
    std::vector<Log> check();
  private:
    std::filesystem::path base_;
    std::unordered_map<std::filesystem::path, std::uintmax_t> cache_;
  };
}