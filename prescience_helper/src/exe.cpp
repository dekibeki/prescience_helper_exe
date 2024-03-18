#include <prescience_helper/sqlite3_wrapper.hpp>
#include <prescience_helper/serialize.hpp>
#include <prescience_helper/ingest.hpp>
#include <prescience_helper/log_finder.hpp>
#include <prescience_helper/sim/on_rails.hpp>


#include <fstream>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <wx/wx.h>
#include <wx/spinctrl.h>

namespace {
  static_assert(std::numeric_limits<float>::is_iec559);
  static_assert(std::numeric_limits<double>::is_iec559);

  constexpr std::size_t EXPECTED_EBON_MIGHT_UPTIME = 22;

  constexpr std::string_view EXPECTED_DB_VERSION = "2";

  constexpr std::string_view init_db =
    "CREATE TABLE Patch("
    " id INTEGER NOT NULL PRIMARY KEY,"
    " expac INTEGER NOT NULL,"
    " patch INTEGER NOT NULL,"
    " minor INTEGER NOT NULL);"
    "INSERT INTO Patch(expac,patch,minor) VALUES"
    " (10,2,0),"
    " (10,2,5);"
    "CREATE TABLE Configs("
    " id INTEGER NOT NULL PRIMARY KEY,"
    " name TEXT NOT NULL,"
    " value TEXT NOT NULL);"
    "INSERT INTO Configs(name,value) VALUES"
    " ('log_location','C:\\Program Files (x86)\\World of Warcraft\\_retail_\\Logs');"
    //" ('version','2');"
    "CREATE TABLE Logs_read("
    " path TEXT NOT NULL PRIMARY KEY,"
    " useful_amount INTEGER NOT NULL,"
    " total_amount INTEGER NOT NULL,"
    " last_patch INTEGER NULL,"
    " FOREIGN KEY(last_patch) REFERENCES Patch(id));"
    "CREATE TABLE Difficulty("
    " id INTEGER NOT NULL PRIMARY KEY,"
    " name TEXT NOT NULL);"
    "INSERT INTO Difficulty(id,name) VALUES"
    " (16,'Mythic'),"
    " (15,'Heroic'),"
    " (14,'Normal');"
    "CREATE TABLE Encounter_type("
    " id INTEGER NOT NULL PRIMARY KEY,"
    " name TEXT NOT NULL);"
    "CREATE TABLE Encounter("
    " id INTEGER NOT NULL PRIMARY KEY,"
    " type INTEGER NOT NULL,"
    " patch INTEGER NOT NULL,"
    " difficulty INTEGER NOT NULL,"
    " start_time INTEGER NOT NULL,"
    " duration_ms INTEGER NOT NULL,"
    " FOREIGN KEY(type) REFERENCES Encounter_type(id),"
    " FOREIGN KEY(patch) REFERENCES Patch(id),"
    " FOREIGN KEY(difficulty) REFERENCES Difficulty(id));"
    "CREATE TABLE Player("
    " id INTEGER NOT NULL PRIMARY KEY,"
    " blizz_guid TEXT NOT NULL);"
    "CREATE TABLE Logged("
    " id INTEGER NOT NULL PRIMARY KEY,"
    " player INTEGER NOT NULL,"
    " spec INT NOT NULL,"
    " encounter BIGINT NOT NULL,"
    " damage BLOB NULL,"
    " stats BLOB NOT NULL,"
    " deaths BLOB NULL,"
    " rezzes BLOB NULL,"
    " FOREIGN KEY(player) REFERENCES Player(id),"
    " FOREIGN KEY(encounter) REFERENCES Encounter(id));";

  constexpr std::uint32_t LOGGED_DAMAGE_FLAG1_SCALES_WITH_PRIMARY = 1 << 0;
  constexpr std::uint32_t LOGGED_DAMAGE_FLAG1_CAN_NOT_CRIT = 1 << 1;
  constexpr std::uint32_t LOGGED_DAMAGE_FLAG1_ALLOW_CLASS_ABILITY_PROCS = 1 << 2;

  struct Patch {
    clogparser::events::Combat_log_version::Build_version build;
    std::int32_t id;
  };

  wxString to_wxString(std::string_view in) {
    return wxString(in.data(), in.length());
  }

  struct File final : public prescience_helper::File {
    static constexpr std::size_t BUFFER_SIZE = 128 * 1024 * 1024; //128mb

    File() {
      buffer.resize(BUFFER_SIZE);
    }

    void open(std::filesystem::path path, std::uintmax_t offset) {
      if (input.is_open()) {
        input.close();
      }
      input.open(path, std::ios::binary | std::ios::in);
      input.seekg(offset);
    }

    std::string_view next() override final {
      input.read(buffer.data(), BUFFER_SIZE);
      const auto read = input.gcount();

      return { buffer.data(), static_cast<std::size_t>(read) };
    }

    std::vector<char> buffer;
    std::ifstream input;
  };

  constexpr std::size_t SIZEOF_DAMAGE_EVENT = 
    sizeof(clogparser::Period::rep)
    + sizeof(double) * 3
    + sizeof(std::uint8_t);

  void serialize(std::vector<prescience_helper::Event<prescience_helper::sim::Damage>> const& in, std::vector<std::byte>& returning) {

    prescience_helper::serialize::Write_buffer buffer{ returning };
    buffer.reserve_more(in.size() * SIZEOF_DAMAGE_EVENT);

    for (auto const& damage : in) {
      const std::uint8_t flags =
        damage.what.allow_class_ability_procs ? LOGGED_DAMAGE_FLAG1_ALLOW_CLASS_ABILITY_PROCS : 0
        | damage.what.can_not_crit ? LOGGED_DAMAGE_FLAG1_CAN_NOT_CRIT : 0
        | damage.what.scales_with_primary ? LOGGED_DAMAGE_FLAG1_SCALES_WITH_PRIMARY : 0;

      buffer.write(
        damage.when.count(),
        damage.what.base_scaling,
        damage.what.amp.crit_amp,
        damage.what.amp.crit_chance_add,
        flags);
    }
  }
  void deserialize(std::span<const std::byte> in, std::vector<prescience_helper::Event<prescience_helper::sim::Damage>>& out) {
    if (in.size() % SIZEOF_DAMAGE_EVENT != 0) {
      throw std::exception{ "In doesn't contain a whole multiple of the event" };
    }

    out.reserve(in.size() / SIZEOF_DAMAGE_EVENT);

    prescience_helper::serialize::Read_buffer buffer{ in };

    while (!buffer.empty()) {
      prescience_helper::Event<prescience_helper::sim::Damage> adding;
      adding.when = clogparser::Period{ buffer.read<clogparser::Period::rep>() };
      adding.what.base_scaling = buffer.read<double>();
      adding.what.amp.crit_amp = buffer.read<double>();
      adding.what.amp.crit_chance_add = buffer.read<double>();

      const auto flags = buffer.read<std::uint8_t>();

      adding.what.allow_class_ability_procs = (flags & LOGGED_DAMAGE_FLAG1_ALLOW_CLASS_ABILITY_PROCS) != 0;
      adding.what.can_not_crit = (flags & LOGGED_DAMAGE_FLAG1_CAN_NOT_CRIT) != 0;
      adding.what.scales_with_primary = (flags & LOGGED_DAMAGE_FLAG1_SCALES_WITH_PRIMARY) != 0;

      out.push_back(std::move(adding));
    }
  }
  constexpr std::size_t SIZEOF_STATS_EVENT =
    sizeof(clogparser::Period::rep)
    + sizeof(prescience_helper::sim::Combat_stats::value_type) * prescience_helper::sim::Combat_stats::size;

  void serialize(std::vector<prescience_helper::Event<prescience_helper::sim::Combat_stats>> const& in, std::vector<std::byte>& returning) {

    prescience_helper::serialize::Write_buffer buffer{ returning };
    buffer.reserve_more(in.size() * SIZEOF_STATS_EVENT);

    for (auto const& event : in) {
      buffer.write(event.when.count());
      for (auto const& stat : event.what) {
        buffer.write(stat);
      }
    }
  }
  void deserialize(std::span<const std::byte> in, std::vector<prescience_helper::Event<prescience_helper::sim::Combat_stats>>& out) {
    if (in.size() % SIZEOF_STATS_EVENT != 0) {
      throw std::exception{ "In doesn't contain a whole multiple of the event" };
    }

    out.reserve(in.size() / SIZEOF_STATS_EVENT);

    prescience_helper::serialize::Read_buffer buffer{ in };

    while (!buffer.empty()) {
      prescience_helper::Event<prescience_helper::sim::Combat_stats> adding;
      adding.when = clogparser::Period{ buffer.read<clogparser::Period::rep>() };
      for (auto& stat : adding.what) {
        stat = buffer.read<double>();
      }
      out.push_back(std::move(adding));
    }
  }
  constexpr std::size_t SIZEOF_DIED_REZZED_EVENT =
    sizeof(clogparser::Period::rep);

  void serialize(std::vector<prescience_helper::Event<void>> const& in, std::vector<std::byte>& returning) {

    prescience_helper::serialize::Write_buffer buffer{ returning };
    buffer.reserve_more(in.size() * SIZEOF_DIED_REZZED_EVENT);

    for (auto const& event : in) {
      buffer.write(event.when.count());
    }
  }
  void deserialize(std::span<const std::byte> in, std::vector<prescience_helper::Event<void>>& out) {
    if (in.size() % SIZEOF_DIED_REZZED_EVENT != 0) {
      throw std::exception{ "In doesn't contain a whole multiple of the event" };
    }

    out.reserve(in.size() / SIZEOF_DIED_REZZED_EVENT);

    prescience_helper::serialize::Read_buffer buffer{ in };

    while (!buffer.empty()) {
      out.push_back(prescience_helper::Event<void>{
        clogparser::Period{ buffer.read<clogparser::Period::rep>() }
      });
    }
  }
  struct Thread_activity {
    std::vector<std::pair<std::string, std::int32_t>> new_encounter_ids;
    bool parsing = false;
    std::uint32_t encounters_read = 0;

    void clear() {
      new_encounter_ids.clear();
      encounters_read = 0;
    }
  };

  struct Parse_thread {
    Parse_thread(std::filesystem::path base, prescience_helper::Db db) :
      db_(std::move(db)),
      insert_encounter_type_stmt_(db_.prepare("INSERT INTO Encounter_type(id,name) VALUES (?,?);")),
      check_for_existing_encounter_stmt_(db_.prepare("SELECT COUNT(1) FROM Encounter WHERE start_time = ? AND type = ? AND difficulty = ? AND patch = ?;")),
      insert_encounter_stmt_(db_.prepare("INSERT INTO Encounter(type, patch, difficulty, start_time, duration_ms) VALUES (?, ?, ?, ?, ?);")),
      find_player_stmt_(db_.prepare("SELECT id FROM Player WHERE blizz_guid = ?;")),
      insert_player_stmt_(db_.prepare("INSERT INTO Player (blizz_guid) VALUES (?);")),
      insert_logged_stmt_(db_.prepare("INSERT INTO Logged (player,spec,encounter,damage,stats,deaths,rezzes) VALUES (?,?,?,?,?,?,?);")),
      update_log_read_(db_.prepare("INSERT INTO Logs_read(path, useful_amount, total_amount,last_patch) VALUES(?, ?, ?, ?) ON CONFLICT(path) DO UPDATE SET useful_amount = excluded.useful_amount, total_amount = excluded.total_amount, last_patch = excluded.last_patch;")),
      log_finder(base),
      base(base) {

      const auto patches_stmt = db_.prepare("SELECT id, expac, patch, minor FROM Patch;");
      patches_stmt.exec<std::int32_t, std::int32_t, std::int32_t, std::int32_t>(
        [this](std::int32_t id, std::int32_t expac, std::int32_t patch, std::int32_t minor) {
          patches_.push_back(Patch{
          clogparser::events::Combat_log_version::Build_version{
            static_cast<std::uint8_t>(expac),
            static_cast<std::uint8_t>(patch),
            static_cast<std::uint8_t>(minor)},
            id,
            });
        });
    }
    prescience_helper::Db db_;
    std::thread thread;

    std::mutex mutex_;
    prescience_helper::Log_finder log_finder;
    bool should_stop = false;
    Thread_activity thread_activity_;
    std::filesystem::path base;

    prescience_helper::Stmt insert_encounter_type_stmt_;
    prescience_helper::Stmt check_for_existing_encounter_stmt_;
    prescience_helper::Stmt insert_encounter_stmt_;
    prescience_helper::Stmt find_player_stmt_;
    prescience_helper::Stmt insert_player_stmt_;
    prescience_helper::Stmt insert_logged_stmt_;
    prescience_helper::Stmt update_log_read_;

    std::vector<Patch> patches_;
    std::unordered_set<std::int32_t> difficulty_ids_;
    std::unordered_set<std::int32_t> encounter_ids_;

    void change_finder_base(std::filesystem::path new_base) {
      std::lock_guard lock{ mutex_ };
      base = std::move(new_base);
    }
    void check_if_finder_base_should_change() {
      std::lock_guard lock{ mutex_ };
      if (base != log_finder.path()) {
        log_finder.change_path(base);
      }
    }

    bool check_if_should_stop(std::uint32_t add_encounters_read = 0) {
      std::lock_guard lock{ mutex_ };
      thread_activity_.encounters_read += add_encounters_read;
      return should_stop;
    }
    void stop() {
      std::lock_guard lock{ mutex_ };
      should_stop = true;
    }

    Thread_activity get_thread_activity() {
      std::lock_guard lock{ mutex_ };
      auto returning{ std::move(thread_activity_) };
      thread_activity_.clear();
      return returning;
    }

    static void run(Parse_thread* state) {
      std::vector<prescience_helper::Log_finder::Log> logs;
      File reader;
      clogparser::String_store strings;

      std::vector<std::byte> serialized_damages;
      std::vector<std::byte> serialized_stats;
      std::vector<std::byte> serialized_deaths;
      std::vector<std::byte> serialized_rezzes;

      for (;;) {
        logs.clear();

        state->check_if_finder_base_should_change();
        state->log_finder.check(logs);

        {
          std::lock_guard lock{ state->mutex_ };
          if (state->should_stop) {
            return;
          }
          state->thread_activity_.parsing = !logs.empty();
        }

        if (logs.empty()) {
          std::this_thread::sleep_for(std::chrono::milliseconds{ 500 });
          continue;
        }

        std::vector<prescience_helper::Encounter> ingested;

        for (auto& log : logs) {
          strings.clear();
          reader.open(log.path, log.old_useful);
          ingested.clear();
          prescience_helper::ingest(reader, strings, ingested);
          bool contains_future = false;
          for (auto const& encounter : ingested) {
            clogparser::events::Combat_log_version::Build_version build{ 0 };
            if (encounter.build) {
              build = *encounter.build;
            } else if (log.last_patch) {
              const auto found = std::find_if(state->patches_.begin(), state->patches_.end(), [id = *log.last_patch](Patch const& patch) {
                return patch.id == id;
                });
              if (found != state->patches_.end()) {
                build = found->build;
              } else {
                //can't find patch, skip
                continue;
              }
            } else {
              //can't find patch, skip
              continue;
            }

            if (encounter.end_byte > log.old_useful) {
              log.old_useful = encounter.end_byte;
              state->log_finder.set_log_read(log.path, encounter.end_byte, log.new_total, log.last_patch);
            }

            const auto is_valid = (build <=> prescience_helper::sim::valid_for);
            if (is_valid == std::strong_ordering::less) {
              continue; //old, ignore
            } else if (is_valid == std::strong_ordering::greater) {
              contains_future = true;
              continue; //new, we can't parse this yet
            }

            const auto simulated = prescience_helper::sim::on_rails::simulate(encounter);

            const auto found_patch = std::find_if(state->patches_.begin(), state->patches_.end(), [build](Patch const& patch) {
              return patch.build == build;
              });

            if (found_patch == state->patches_.end()) {
              fprintf(stderr, "Unknown patch: %hhu.%hhu.%hhu\n",
                build.expac,
                build.patch,
                build.minor);
              continue;
            }

            log.last_patch = found_patch->id;

            if (state->difficulty_ids_.count((std::int32_t)encounter.start.difficulty_id) == 0) {
              continue;
            }

            //correct encounter starttime

            const std::chrono::milliseconds time =
              std::chrono::days{ 31 } * encounter.start_time.month
              + std::chrono::days{ encounter.start_time.day }
              + std::chrono::hours{ encounter.start_time.hour }
              + std::chrono::minutes{ encounter.start_time.minute }
              + std::chrono::seconds{ encounter.start_time.second }
            + std::chrono::milliseconds{ encounter.start_time.millisecond };

            state->db_.begin();

            if (state->encounter_ids_.count(encounter.start.encounter_id) == 0) {
              state->insert_encounter_type_stmt_.exec([]() {}, encounter.start.encounter_id, encounter.start.encounter_name);
              state->encounter_ids_.insert(encounter.start.encounter_id);
              {
                std::lock_guard lock{ state->mutex_ };
                state->thread_activity_.new_encounter_ids.push_back({ std::string{encounter.start.encounter_name}, encounter.start.encounter_id });
              }
            } else { //if the encounter type didn't exist, the encounter can't have existed, so don't check
              std::int32_t count = 0;
              state->check_for_existing_encounter_stmt_.exec<std::int32_t>([&count](std::int32_t res) {
                count = res;
                }, time.count(), encounter.start.encounter_id, (std::int32_t)encounter.start.difficulty_id, found_patch->id);

              if (count != 0) {
                state->db_.rollback();
                continue;
              }
            }

            state->insert_encounter_stmt_.exec([]() {},
              encounter.start.encounter_id,
              found_patch->id,
              (std::int32_t)encounter.start.difficulty_id,
              time.count(),
              std::chrono::duration_cast<std::chrono::milliseconds>(encounter.end_time - encounter.start_time).count());

            const auto encounter_id = state->db_.last_insert_rowid();

            for (auto const& player : simulated.players) {
              if (player.damage_events.empty()) {
                //if player did no damage (e.g. reset or maybe a carry) ignore this pull
                continue;
              }
              std::optional<std::int32_t> player_id = std::nullopt;
              state->find_player_stmt_.exec<std::int32_t>([&player_id](std::int32_t id) {
                player_id = id;
                }, player.ingest_player->info.guid);
              if (!player_id.has_value()) {
                state->insert_player_stmt_.exec([]() {}, player.ingest_player->info.guid);
                player_id = state->db_.last_insert_rowid();
              }

              serialized_damages.clear();
              serialize(player.damage_events, serialized_damages);
              serialized_stats.clear();
              serialize(player.stat_events, serialized_stats);
              serialized_deaths.clear();
              serialize(player.died, serialized_deaths);
              serialized_rezzes.clear();
              serialize(player.rezzed, serialized_rezzes);

              state->insert_logged_stmt_.exec([]() {}, 
                *player_id, (std::int32_t)player.ingest_player->info.current_spec_id, encounter_id, serialized_damages, serialized_stats, serialized_deaths, serialized_rezzes);
            }
            state->db_.commit();

            if (state->check_if_should_stop()) {
              return;
            }
          }
          if (!contains_future) { //if we contain encounters from the future, don't persist that we've read them. Then later on we'll re read them and hopefully be able to understand them
            const auto path_string = log.path.string();
            state->update_log_read_.exec([]() {}, path_string, (std::int64_t)log.old_useful, (std::int64_t)log.new_total, log.last_patch);
          }
        }
      }
    }
  };

  class Main_frame : public wxFrame {
  public:
    Main_frame(prescience_helper::Db our_db, prescience_helper::Db parser_db) :
      wxFrame(nullptr, wxID_ANY, "Prescience Helper"),
      db_(std::move(our_db)),
      settings_(db_),
      parse_thread_("./", std::move(parser_db)),
      get_member_logged_(db_.prepare(
        "SELECT Encounter.duration_ms,Logged.damage,Logged.stats,Logged.deaths,Logged.rezzes FROM Logged"
        " INNER JOIN Encounter ON Logged.encounter = Encounter.id"
        " INNER JOIN Player ON Logged.player = Player.id"
        " WHERE Player.blizz_guid = ? AND Logged.spec = ? AND Encounter.type = ? AND Encounter.difficulty = ?"
        " ORDER BY Encounter.start_time DESC"
        " LIMIT 10;")),
    get_aug_logged_(db_.prepare(
      "SELECT Encounter.duration_ms,Logged.stats,Logged.deaths,Logged.rezzes FROM Logged"
      " INNER JOIN Encounter ON Logged.encounter = Encounter.id"
      " INNER JOIN Player ON Logged.player = Player.id"
      " WHERE Player.blizz_guid = ? AND Logged.spec = 1473 AND Encounter.type = ? AND Encounter.difficulty = ?"
      " ORDER BY Encounter.start_time DESC"
      " LIMIT 10;")) {

      wxBoxSizer* top_sizer = new wxBoxSizer(wxVERTICAL);
      this->SetSizer(top_sizer);
      this->SetAutoLayout(true);

      wxPanel* log_location_panel = new wxPanel(this);
      top_sizer->Add(log_location_panel, wxSizerFlags().Expand());

      wxBoxSizer* log_location_sizer = new wxBoxSizer(wxHORIZONTAL);
      log_location_panel->SetSizer(log_location_sizer);
      log_location_panel->SetAutoLayout(true);

      log_location_sizer->Add(new wxButton(log_location_panel, CHILD_IDS::log_location, "Set log location"));

      std::string_view log_location = settings_.get("log_location");
      parse_thread_.log_finder.change_path(log_location);
      parse_thread_.base = log_location;
      log_location_text_ = new wxTextCtrl(log_location_panel, -1, to_wxString(log_location), wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
      log_location_sizer->Add(log_location_text_, 1, wxSizerFlags().Expand().GetFlags());
      poll_threads_timer_.SetOwner(this, poll_threads_timer);

      db_.exec<std::string_view, std::int64_t, std::int64_t,std::optional<std::int32_t>>("SELECT path,useful_amount,total_amount,last_patch FROM Logs_read;",
        [this](std::string_view path, std::int64_t useful_amount, std::int64_t total_amount, std::optional<std::int32_t> last_patch) {
          parse_thread_.log_finder.set_log_read(path, useful_amount, total_amount, last_patch);
        });

      wxPanel* choice_panel = new wxPanel(this);
      top_sizer->Add(choice_panel, wxSizerFlags().Expand());

      wxBoxSizer* choice_sizer = new wxBoxSizer(wxHORIZONTAL);
      choice_panel->SetSizer(choice_sizer);
      choice_panel->SetAutoLayout(true);

      choice_sizer->Add(new wxStaticText(choice_panel, -1, " Size of computed window (in 100ms): "), wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));

      window_size_ = new wxSpinCtrl(choice_panel, CHILD_IDS::window_size, "10");
      window_size_->SetRange(1, 300);
      choice_sizer->Add(window_size_);

      choice_sizer->Add(new wxStaticText(choice_panel, -1, " Difficulty: "), wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));

      db_.exec<std::int32_t, std::string_view>("SELECT id, name FROM Difficulty",
        [this](std::int32_t id, std::string_view name) {
          difficulty_to_id_[std::string{ name }] = id;
          difficulties_.Add(wxString(name.data(), name.length()));
          parse_thread_.difficulty_ids_.insert(id); //we can do this before we start the thread
        });
      difficulty_choice_ = new wxChoice(choice_panel, CHILD_IDS::difficulties, wxDefaultPosition, wxDefaultSize, difficulties_);
      choice_sizer->Add(difficulty_choice_);

      choice_sizer->Add(new wxStaticText(choice_panel, -1, " Encounter: "), wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));

      db_.exec<std::int32_t, std::string_view>("SELECT id,name FROM Encounter_type;",
        [this](std::int32_t id, std::string_view name) {
          encounter_name_to_id_[std::string{ name }] = id;
          encounters_.Add(wxString::FromAscii(name.data(), name.length()));
          parse_thread_.encounter_ids_.insert(id); //we can do this before we start the thread
        });
      encounter_choice_ = new wxChoice(choice_panel, CHILD_IDS::encounters, wxDefaultPosition, wxDefaultSize, encounters_);
      choice_sizer->Add(encounter_choice_, 1, wxSizerFlags().Expand().GetFlags());

      wxPanel* input_panel = new wxPanel(this);
      top_sizer->Add(input_panel, wxSizerFlags().Expand());

      wxBoxSizer* input_sizer = new wxBoxSizer(wxHORIZONTAL);
      input_panel->SetSizer(input_sizer);
      input_panel->SetAutoLayout(true);

      input_sizer->Add(new wxStaticText(input_panel, -1, " Input: "), wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));

      input_text_ = new wxTextCtrl(input_panel, CHILD_IDS::input_text);
      input_sizer->Add(input_text_, 1, wxSizerFlags().Expand().GetFlags());

      wxPanel* output_panel = new wxPanel(this);
      top_sizer->Add(output_panel, 1, wxSizerFlags().Expand().GetFlags());

      wxBoxSizer* output_sizer = new wxBoxSizer(wxHORIZONTAL);
      output_panel->SetSizer(output_sizer);
      output_panel->SetAutoLayout(true);

      output_sizer->Add(new wxStaticText(output_panel, -1, " Output: "), wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));

      output_text_ = new wxTextCtrl(output_panel, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_RICH | wxTE_MULTILINE);
      output_text_->SetEditable(false);
      output_sizer->Add(output_text_, 1, wxSizerFlags().Expand().GetFlags());

      wxPanel* gen_info_panel = new wxPanel(this);
      top_sizer->Add(gen_info_panel, wxSizerFlags().Expand());

      wxBoxSizer* gen_info_sizer = new wxBoxSizer(wxHORIZONTAL);
      gen_info_panel->SetSizer(gen_info_sizer);
      gen_info_panel->SetAutoLayout(true);

      gen_info_sizer->Add(new wxStaticText(gen_info_panel, -1, " Input generation info: "), wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));
      gen_info_text_ = new wxTextCtrl(gen_info_panel, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
      gen_info_sizer->Add(gen_info_text_, 1, wxSizerFlags().Expand().GetFlags());

      wxPanel* parse_info_panel = new wxPanel(this);
      top_sizer->Add(parse_info_panel, wxSizerFlags().Expand());

      wxBoxSizer* parse_info_sizer = new wxBoxSizer(wxHORIZONTAL);
      parse_info_panel->SetSizer(parse_info_sizer);
      parse_info_panel->SetAutoLayout(true);

      parse_info_sizer->Add(new wxStaticText(parse_info_panel, -1, " Parse info: "), wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));
      parse_info_text_ = new wxTextCtrl(parse_info_panel, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
      parse_info_sizer->Add(parse_info_text_, 1, wxSizerFlags().Expand().GetFlags());

      this->Fit();

      parse_thread_.thread = std::thread{ Parse_thread::run, &parse_thread_ };
      poll_threads_timer_.Start(1 * 1000, wxTIMER_CONTINUOUS);
    }

    void on_change_difficulty(wxCommandEvent& ev) {
      set_output();
    }

    void on_change_encounter(wxCommandEvent& ev) {
      set_output();
    }

    void on_press_change_log_location(wxCommandEvent& ev) {
      wxDirDialog dlg(NULL, "Choose input directory", log_location_text_->GetValue(), wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
      const auto res = dlg.ShowModal();

      if (res == wxID_OK) {
        log_location_text_->ChangeValue(dlg.GetPath());
        std::string path = dlg.GetPath().ToStdString();
        settings_.set("log_location", path);
        parse_thread_.change_finder_base(std::move(path));
      }
    }

    void on_poll_threads_timer(wxTimerEvent&) {
      const auto thread_activity = parse_thread_.get_thread_activity();

      if (!thread_activity.new_encounter_ids.empty()) {
        for (auto const& [encounter_name, encounter_id] : thread_activity.new_encounter_ids) {
          encounter_name_to_id_.emplace(encounter_name, encounter_id);
          encounter_choice_->Append(to_wxString(encounter_name));
        }
      }

      if (thread_activity.encounters_read == 0 && thread_activity.parsing) {
          parse_info_text_->SetValue("Parsing");
      } else if (thread_activity.encounters_read != 0) {
        std::stringstream output;
        output << 
          "Parsed " << thread_activity.encounters_read << " encounter(s)"
          " at " << wxDateTime::Now().FormatTime().ToStdString();

        parse_info_text_->SetValue(output.str());
        parse_thread_last_info_ = output.str();
        set_output();
      } else if(parse_thread_was_parsing_) {
        assert(!thread_activity.parsing);
        parse_info_text_->SetValue(parse_thread_last_info_);
      }

      parse_thread_was_parsing_ = thread_activity.parsing;
    }

    void on_input_change(wxCommandEvent&) {
      set_output();
    }

    void on_window_size_change(wxSpinEvent&) {
      set_output();
    }

    void set_output() {
      const auto benchmark_now = std::chrono::high_resolution_clock::now();

      const auto found_difficulty = difficulty_to_id_.find(difficulty_choice_->GetStringSelection().ToStdString());
      if (found_difficulty == difficulty_to_id_.end()) {
        gen_info_text_->SetValue("Invalid difficulty");
        output_text_->SetValue("");
        return;
      }

      const auto found_encounter = encounter_name_to_id_.find(encounter_choice_->GetStringSelection().ToStdString());
      if (found_encounter == encounter_name_to_id_.end()) {
        gen_info_text_->SetValue("Invalid encounter");
        output_text_->SetValue("");
        return;
      }

      const clogparser::Period window_size = std::chrono::milliseconds{ window_size_->GetValue() * 100 };

      const std::string input = input_text_->GetValue().ToStdString();
      std::string_view input_working = input;
      const auto found_version_delim = input_working.find('?');
      if (found_version_delim == std::string_view::npos) {
        gen_info_text_->SetValue("Couldn't find a version in the input string");
        output_text_->SetValue("");
        return;
      }
      const std::string_view version = input_working.substr(0, found_version_delim);
      input_working = input_working.substr(found_version_delim + 1);
      if (version != "2") {
        gen_info_text_->SetValue("Unsupported input version. Input has version '" + std::string{ version } + "' while we expected '2'");
        output_text_->SetValue("");
        return;
      }
      std::vector<std::string_view> member_raw = clogparser::helpers::parse_array(input_working);

      if (member_raw.empty() || (member_raw.size() == 1 && member_raw.front() == "")) {
        gen_info_text_->SetValue("Invalid input");
        output_text_->SetValue("");
        return;
      }

      const std::string_view aug_guid = member_raw[0];

      std::vector<clogparser::Period> durations;
      std::vector<std::vector<prescience_helper::Event<prescience_helper::sim::Damage>>> damages;
      std::vector<std::vector<prescience_helper::Event<prescience_helper::sim::Combat_stats>>> stats;
      std::vector<std::vector<prescience_helper::Event<void>>> deaths;
      std::vector<std::vector<prescience_helper::Event<void>>> rezzes;
      std::vector<double> weights;

      get_aug_logged_.exec<std::int64_t, std::span<const std::byte>, std::span<const std::byte>, std::span<const std::byte>>(
        [&durations, &stats, &deaths, &rezzes, &weights](std::int64_t duration, std::span<const std::byte> stat, std::span<const std::byte> death, std::span<const std::byte> rezz) {
          durations.push_back(std::chrono::duration_cast<clogparser::Period>(std::chrono::milliseconds{ duration }));

          stats.emplace_back();
          deserialize(stat, stats.back());

          deaths.emplace_back();
          deserialize(death, deaths.back());

          rezzes.emplace_back();
          deserialize(rezz, rezzes.back());

          weights.push_back(1);
        }, aug_guid, found_encounter->second, found_difficulty->second);

      const auto agged_aug_stats = prescience_helper::sim::on_rails::aggregate_stats(durations, stats, deaths, rezzes, weights);

      std::vector<std::string_view> member_members;

      std::stringstream output;

      output <<
        "2?";
      std::vector<std::byte> payload_underlying;
      prescience_helper::serialize::Write_buffer payload_raw{ payload_underlying };
      payload_raw.write<std::uint8_t>(std::chrono::duration_cast<std::chrono::milliseconds>(window_size).count() / 100);
      payload_raw.write<std::uint8_t>(member_raw.size() - 1);
      for (std::size_t i = 1; i < member_raw.size(); ++i) {
        member_members.clear();
        clogparser::helpers::parse_array(member_members, member_raw[i]);

        if (member_members.size() != 3) {
          gen_info_text_->SetValue("Invalid member info, didn't have exactly 3 values");
          output_text_->SetValue("");
          return;
        }

        const std::string_view player_name = member_members[1];

        const auto guid = member_members[0];
        std::int32_t spec_id = -1;
        try {
          spec_id = clogparser::helpers::parseInt<std::int32_t>(member_members[2]);
        } catch (...) {
          gen_info_text_->SetValue("Couldn't parse spec id");
          output_text_->SetValue("");
          return;
        }

        const auto first_dash = guid.find('-');
        if (first_dash == std::string_view::npos) {
          gen_info_text_->SetValue("Unexpected GUID format in '" + std::string{ guid } + '\'');
          output_text_->SetValue("");
          return;
        }
        const auto second_dash = guid.find('-', first_dash + 1);
        if (second_dash == std::string_view::npos) {
          gen_info_text_->SetValue("Unexpected GUID format in '" + std::string{ guid } + '\'');
          output_text_->SetValue("");
          return;
        }
        const std::string_view guid_server_id_str = guid.substr(first_dash + 1, second_dash - first_dash - 1);
        const std::string_view guid_player_uid_str = guid.substr(second_dash + 1);

        try {
          const std::uint16_t guid_server_id = clogparser::helpers::parseInt<std::uint16_t>(guid_server_id_str);
          payload_raw.write(guid_server_id);
        } catch (std::exception const&) {
          gen_info_text_->SetValue("Unexpected GUID format in '" + std::string{ guid } + '\'');
          output_text_->SetValue("");
          return;
        }

        std::uint32_t guid_player_uid = 0;
        std::from_chars_result guid_player_uid_res = std::from_chars(guid_player_uid_str.data(), guid_player_uid_str.data() + guid_player_uid_str.size(), guid_player_uid, 16);
        if (guid_player_uid_res.ec != std::errc()) {
          gen_info_text_->SetValue("Unexpected GUID format in '" + std::string{ guid } + '\'');
          output_text_->SetValue("");
          return;
        }
        payload_raw.write(guid_player_uid);

        durations.clear();
        damages.clear();
        stats.clear();
        deaths.clear();
        rezzes.clear();
        weights.clear();
        get_member_logged_.exec<std::int64_t, std::span<const std::byte>, std::span<const std::byte>, std::span<const std::byte>, std::span<const std::byte>>(
          [&durations, &damages, &stats, &deaths, &rezzes, &weights](std::int64_t duration, std::span<const std::byte> damage, std::span<const std::byte> stat, std::span<const std::byte> death, std::span<const std::byte> rezz) {
            durations.push_back(std::chrono::duration_cast<clogparser::Period>(std::chrono::milliseconds{ duration }));

            damages.emplace_back();
            deserialize(damage, damages.back());

            stats.emplace_back();
            deserialize(stat, stats.back());

            deaths.emplace_back();
            deserialize(death, deaths.back());

            rezzes.emplace_back();
            deserialize(rezz, rezzes.back());

            weights.push_back(1);
          }, guid, spec_id, found_encounter->second, found_difficulty->second);

        const auto agged_damage = prescience_helper::sim::on_rails::aggregate_damage(agged_aug_stats, durations, damages, stats, deaths, rezzes, weights, true, window_size);

        std::int64_t prev_window{ -1 };
        for (std::size_t i = 0; i < agged_damage.size(); ++i) {
          const auto cur_window = agged_damage[i].when / window_size;
          auto delta_window = cur_window - prev_window;

          do {
            const auto our_window = std::min<decltype(delta_window)>(delta_window, std::numeric_limits<std::uint8_t>::max());
            payload_raw.write_clamped<std::uint8_t>(our_window - 1);
            payload_raw.write_clamped<std::uint16_t>(agged_damage[i].what.base / 1000);
            payload_raw.write_clamped<std::uint8_t>((agged_damage[i].what.with_ebon_mult - 1) * 100);
            payload_raw.write_clamped<std::uint8_t>((agged_damage[i].what.with_prescience_mult - 1) * 100);
            payload_raw.write_clamped<std::uint8_t>((agged_damage[i].what.with_shifting_sands_mult - 1) * 100);

            delta_window -= our_window;
          } while (delta_window > std::numeric_limits<std::uint8_t>::max());
          prev_window = cur_window;
        }
        //EOR special character
        payload_raw.write<std::uint8_t>(std::numeric_limits<std::uint8_t>::max());
      }

      prescience_helper::serialize::to_ascii_85(output, payload_underlying);
      const auto output_str = output.str();

      const auto benchmark_delta = std::chrono::high_resolution_clock::now() - benchmark_now;

      output_text_->SetValue(output_str);
      output_text_->SelectAll();
      output_text_->SetFocus();
      std::stringstream info_text;
      info_text <<
        "Success at " << wxDateTime::Now().FormatTime() <<
        " in " << std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(benchmark_delta).count()) << "ms";
      gen_info_text_->SetValue(info_text.str());
    }

    ~Main_frame() {
      parse_thread_.stop();
      parse_thread_.thread.join();
    }

    enum CHILD_IDS : int {
      difficulties = wxID_HIGHEST + 1,
      encounters,
      log_location,
      poll_threads_timer,
      input_text,
      window_size,
    };
  private:
    wxDECLARE_EVENT_TABLE();

    prescience_helper::Db db_;

    Parse_thread parse_thread_;
    std::string parse_thread_last_info_;
    bool parse_thread_was_parsing_ = false;
    prescience_helper::Settings settings_;

    wxTextCtrl* log_location_text_;
    wxTimer poll_threads_timer_;

    wxSpinCtrl* window_size_;

    wxArrayString difficulties_;
    wxChoice* difficulty_choice_;
    std::unordered_map<std::string, std::int32_t> difficulty_to_id_;


    wxArrayString encounters_;
    wxChoice* encounter_choice_;
    std::unordered_map<std::string, std::int32_t> encounter_name_to_id_;

    wxTextCtrl* input_text_;
    wxTextCtrl* output_text_;
    wxTextCtrl* gen_info_text_;
    wxTextCtrl* parse_info_text_;


    prescience_helper::Stmt get_aug_logged_;
    prescience_helper::Stmt get_member_logged_;
  };

  class Prescience_helper : public wxApp {
  public:
    bool OnInit() override {
      sqlite3* db_raw = nullptr;
      if (sqlite3_open_v2("./prescience_helper.db", &db_raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr) != SQLITE_OK) {
        wxMessageDialog modal{ nullptr,
            "Couldn't open database\n"
            "\n"
            "Could not open our database: prescience_helper.db\n"
            "Prescience Helper will close.",
            "Couldn't open database" };

        modal.ShowModal();
        return false;
      }

      prescience_helper::Db frame_db(db_raw);

      frame_db.exec("PRAGMA foreign_keys = ON;", []() {});

      if (sqlite3_exec(db_raw, "SELECT COUNT(1) FROM Patch;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        if (sqlite3_exec(db_raw, init_db.data(), nullptr, nullptr, nullptr) != SQLITE_OK) {
          wxMessageDialog modal{ nullptr,
            "Couldn't create tables in database\n"
            "\n"
            "We couldn't create tables in the database for our initial population. This is very wrong."
            "Prescience Helper will close.",
            "Couldn't create tables in database" };

          modal.ShowModal();

          return false;
        }
      } else {
        bool correct_version = false;
        frame_db.exec<std::string_view>("SELECT value FROM Configs WHERE name='version';", [&correct_version](std::string_view version) {
          correct_version = (version == EXPECTED_DB_VERSION);
          });

        if (!correct_version) {
          wxMessageDialog modal{ nullptr,
            "Incompatible database version\n"
            "\n"
            "Your database (prescience_helper.db) version is not compatible with this version of Prescience Helper. This should only happen after an update.\n"
            "\n"
            "You must remove the old database so that we can recreate it.\n"
            "Save it somewhere if you want to keep it in case the recreation doesn't work. Then delete it from this folder.\n"
            "Prescience Helper will close.",
            "Incompatible database version" };

          modal.ShowModal();

          return false;
        }
      }

      if(sqlite3_open_v2("./prescience_helper.db", &db_raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr) != SQLITE_OK) {
        wxMessageDialog modal{ nullptr,
            "Couldn't open database\n"
            "\n"
            "Could not open our database: prescience_helper.db\n"
            "Prescience Helper will close.",
            "Couldn't open database" };

        modal.ShowModal();
        return false;
      }

      prescience_helper::Db parse_db{ db_raw };
      Main_frame* frame = new Main_frame(std::move(frame_db), std::move(parse_db));
      frame->Show();
      return true;
    }
  };
}

wxBEGIN_EVENT_TABLE(Main_frame, wxFrame)
  EVT_CHOICE(Main_frame::CHILD_IDS::difficulties, Main_frame::on_change_difficulty)
  EVT_CHOICE(Main_frame::CHILD_IDS::encounters, Main_frame::on_change_encounter)
  EVT_BUTTON(Main_frame::CHILD_IDS::log_location, Main_frame::on_press_change_log_location)
  EVT_TIMER(Main_frame::CHILD_IDS::poll_threads_timer, Main_frame::on_poll_threads_timer)
  EVT_TEXT(Main_frame::CHILD_IDS::input_text, Main_frame::on_input_change)
  EVT_SPINCTRL(Main_frame::CHILD_IDS::window_size, Main_frame::on_window_size_change)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP(Prescience_helper);