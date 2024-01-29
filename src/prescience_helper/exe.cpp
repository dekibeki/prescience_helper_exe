#include <prescience_helper/sqlite3_wrapper.hpp>
#include <prescience_helper/ingest.hpp>
#include <prescience_helper/log_finder.hpp>


#include <fstream>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <wx/wx.h>
#include <wx/spinctrl.h>

namespace {
  constexpr std::size_t EXPECTED_EBON_MIGHT_UPTIME = 22;

  constexpr std::string_view init_db =
    "CREATE TABLE Patch("
    "id INTEGER NOT NULL PRIMARY KEY,"
    "expac INTEGER NOT NULL,"
    "patch INTEGER NOT NULL,"
    "minor INTEGER NOT NULL);"
    "INSERT INTO Patch(expac,patch,minor) VALUES"
    "(10,2,0),"
    "(10,2,5);"
    "CREATE TABLE Configs("
    "id INTEGER NOT NULL PRIMARY KEY,"
    "name TEXT NOT NULL,"
    "value TEXT NOT NULL);"
    "INSERT INTO Configs(name,value) VALUES"
    "('log_location','C:\\Program Files (x86)\\World of Warcraft\\_retail_\\Logs');"
    "CREATE TABLE Logs_read("
    "path TEXT NOT NULL PRIMARY KEY,"
    "amount INTEGER NOT NULL);"
    "CREATE TABLE Difficulty("
    "id INTEGER NOT NULL PRIMARY KEY,"
    "name TEXT NOT NULL);"
    "INSERT INTO Difficulty(id,name) VALUES"
    "(16,'Mythic'),"
    "(15,'Heroic'),"
    "(14,'Normal');"
    "CREATE TABLE Encounter_type("
    "id INTEGER NOT NULL PRIMARY KEY,"
    "name TEXT NOT NULL);"
    "CREATE TABLE Encounter("
    "id INTEGER NOT NULL PRIMARY KEY,"
    "type INTEGER NOT NULL,"
    "patch INTEGER NOT NULL,"
    "difficulty INTEGER NOT NULL,"
    "start_time INTEGER NOT NULL,"
    "FOREIGN KEY(type) REFERENCES Encounter_type(id),"
    "FOREIGN KEY(patch) REFERENCES Patch(id),"
    "FOREIGN KEY(difficulty) REFERENCES Difficulty(id));"
    "CREATE TABLE Player("
    "id INTEGER NOT NULL PRIMARY KEY,"
    "blizz_guid TEXT NOT NULL);"
    "CREATE TABLE Logged("
    "id INTEGER NOT NULL PRIMARY KEY,"
    "player INTEGER NOT NULL,"
    "spec INT NOT NULL,"
    "encounter BIGINT NOT NULL,"
    "amount TEXT NOT NULL,"
    "FOREIGN KEY(player) REFERENCES Player(id),"
    "FOREIGN KEY(encounter) REFERENCES Encounter(id));";

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

    void open(std::filesystem::path path) {
      if (input.is_open()) {
        input.close();
      }
      input.open(path);
    }

    std::string_view next() override final {
      input.read(buffer.data(), BUFFER_SIZE);
      const auto read = input.gcount();

      return { buffer.data(), static_cast<std::size_t>(read) };
    }

    std::vector<char> buffer;
    std::ifstream input;
  };

  struct Raid_member {
    std::size_t prio = 0;
    std::string_view guid;
    std::int32_t spec_id;
    std::vector<std::int64_t> amounts;
    double average = 0;
    std::int64_t amount_for_time = 0;
  };

  struct Time_info {
    std::size_t multiplier = 0;
    std::int64_t amount = 0;
  };

  struct Parse_thread {
    Parse_thread(std::filesystem::path base) :
      log_finder(base),
      base(base) {

    }
    std::thread thread;
    std::mutex transfer_mutex;
    prescience_helper::Log_finder log_finder;
    bool should_stop = false;
    bool parsing = false;
    std::vector<prescience_helper::Log_finder::Log> logs;
    std::vector<prescience_helper::Encounter> parsed;
    std::filesystem::path base;

    void check_if_finder_base_should_change() {
      std::lock_guard lock{ transfer_mutex };
      if (base != log_finder.path()) {
        log_finder.change_path(base);
      }
    }

    static void run(Parse_thread* state) {
      std::vector<prescience_helper::Log_finder::Log> logs;
      File reader;
      clogparser::String_store strings;
      for (;;) {
        logs.clear();

        state->check_if_finder_base_should_change();
        state->log_finder.check(logs);

        {
          std::lock_guard lock{ state->transfer_mutex };
          if (state->should_stop) {
            return;
          }
          state->parsing = !logs.empty();
        }

        if (logs.empty()) {
          
          std::this_thread::sleep_for(std::chrono::seconds{ 1 });
          continue;
        }

        for (auto const& log : logs) {
          reader.open(log.path);
          const auto ingested = prescience_helper::ingest(reader, strings);
          {
            std::lock_guard lock{ state->transfer_mutex };
            if (state->should_stop) {
              return;
            }
            state->parsed.insert(state->parsed.end(), ingested.begin(), ingested.end());
            state->logs.push_back(log);
          }
        }
      }
    }
  };

  class Main_frame : public wxFrame {
  public:
    Main_frame(sqlite3* db) :
      wxFrame(nullptr, wxID_ANY, "Prescience Helper"),
      settings_(db),
      parse_thread_("./") {
      db_ = db;

      const auto patches_stmt = prescience_helper::prepare(db_, "SELECT id, expac, patch, minor FROM Patch;");
      exec<std::int32_t, std::int32_t, std::int32_t, std::int32_t>(patches_stmt,
        [this](std::int32_t id, std::int32_t expac, std::int32_t patch, std::int32_t minor) {
          patches_.push_back(Patch{
          clogparser::events::Combat_log_version::Build_version{
            static_cast<std::uint8_t>(expac),
            static_cast<std::uint8_t>(patch),
            static_cast<std::uint8_t>(minor)},
            id,
            });
        });

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
      log_finder_timer_.SetOwner(this, log_finder_timer);
      
      parse_thread_.thread = std::thread{ Parse_thread::run, &parse_thread_ };
      log_finder_timer_.Start(1 * 1000, wxTIMER_CONTINUOUS);

      const auto logs_read = prescience_helper::prepare(db_, "SELECT path,amount FROM Logs_read;");
      prescience_helper::exec<std::string_view, std::int64_t>(logs_read, [this](std::string_view path, std::int64_t amount) {
        parse_thread_.log_finder.set_log_read(path, amount);
        });

      wxPanel* choice_panel = new wxPanel(this);
      top_sizer->Add(choice_panel, wxSizerFlags().Expand());

      wxBoxSizer* choice_sizer = new wxBoxSizer(wxHORIZONTAL);
      choice_panel->SetSizer(choice_sizer);
      choice_panel->SetAutoLayout(true);

      choice_sizer->Add(new wxStaticText(choice_panel, -1, " Number of alternatives per time: "), wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));

      alternatives_per_time_ = new wxSpinCtrl(choice_panel, CHILD_IDS::alternatives_per_time, "8");
      alternatives_per_time_->SetRange(1, 14);
      choice_sizer->Add(alternatives_per_time_);

      choice_sizer->Add(new wxStaticText(choice_panel, -1, " Difficulty: "), wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));

      const auto get_difficulties = prescience_helper::prepare(db_, "SELECT id, name FROM Difficulty");
      prescience_helper::exec<std::int32_t, std::string_view>(get_difficulties, [this](std::int32_t id, std::string_view name) {
        difficulty_to_id_[std::string{ name }] = id;
        difficulties_.Add(wxString(name.data(), name.length()));
        difficulty_ids_.insert(id);
        });
      difficulty_choice_ = new wxChoice(choice_panel, CHILD_IDS::difficulties, wxDefaultPosition, wxDefaultSize, difficulties_);
      choice_sizer->Add(difficulty_choice_);

      choice_sizer->Add(new wxStaticText(choice_panel, -1, " Encounter: "), wxSizerFlags().Align(wxALIGN_CENTER_VERTICAL));

      const auto get_bosses_stmt = prescience_helper::prepare(db_, "SELECT id,name FROM Encounter_type;");
      prescience_helper::exec<std::int32_t, std::string_view>(get_bosses_stmt, [this](std::int32_t id, std::string_view name) {
        encounter_name_to_id_[std::string{ name }] = id;
        encounters_.Add(wxString::FromAscii(name.data(), name.length()));
        encounter_ids_.insert(id);
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

      output_text_ = new wxTextCtrl(output_panel, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_CHARWRAP);
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

      insert_encounter_type_stmt_ = prescience_helper::prepare(db_, "INSERT INTO Encounter_type(id,name) VALUES (?,?);");
      check_for_existing_encounter_stmt_ = prescience_helper::prepare(db_, "SELECT COUNT(1) FROM Encounter WHERE start_time = ? AND type = ? AND difficulty = ? AND patch = ?;");
      insert_encounter_stmt_ = prescience_helper::prepare(db_, "INSERT INTO Encounter(type, patch, difficulty, start_time) VALUES (?, ?, ?, ?);");
      find_player_stmt_ = prescience_helper::prepare(db_, "SELECT id FROM Player WHERE blizz_guid = ?;");
      insert_player_stmt_ = prescience_helper::prepare(db_, "INSERT INTO Player (blizz_guid) VALUES (?);");
      insert_logged_stmt_ = prescience_helper::prepare(db_, "INSERT INTO Logged (player,spec,encounter,amount) VALUES (?,?,?,?);");
      update_log_read_ = prescience_helper::prepare(db_, "INSERT INTO Logs_read(path, amount) VALUES(?, ?) ON CONFLICT(path) DO UPDATE SET amount = excluded.amount;");

      get_logged_ = prescience_helper::prepare(db_, "SELECT Logged.amount FROM Logged"
        " INNER JOIN Encounter ON Logged.encounter = Encounter.id"
        " INNER JOIN Player ON Logged.player = Player.id"
        " WHERE Player.blizz_guid = ? AND Logged.spec = ? AND Encounter.type = ? AND Encounter.difficulty = ?"
        " ORDER BY Encounter.start_time ASC;");
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
        {
          std::lock_guard lock{ parse_thread_.transfer_mutex };
          parse_thread_.base = std::move(path);
        }
      }
    }

    void on_log_finder_timer(wxTimerEvent&) {
      std::vector<prescience_helper::Encounter> parsed;
      std::vector<prescience_helper::Log_finder::Log> logs;
      bool parsing = false;
      {
        std::lock_guard lock{ parse_thread_.transfer_mutex };
        parsed = std::move(parse_thread_.parsed);
        parse_thread_.parsed.clear();
        logs = std::move(parse_thread_.logs);
        parse_thread_.logs.clear();
        parsing = parse_thread_.parsing;
      }

      if (parsed.empty() && logs.empty()) {
        if (parsing) {
          parse_info_text_->SetValue("Parsing");
        }
        return;
      }

      for (auto const& encounter : parsed) {
        if (!encounter.build) {
          //ignore encounters without a build atm
          continue;
        }

        const auto found_patch = std::find_if(patches_.begin(), patches_.end(), [&encounter](Patch const& patch) {
          return patch.build == *encounter.build;
          });

        if (found_patch == patches_.end()) {
          fprintf(stderr, "Unknown patch: %hhu.%hhu.%hhu\n",
            encounter.build->expac,
            encounter.build->patch,
            encounter.build->minor);
          continue;
        }

        if (difficulty_ids_.count((std::int32_t)encounter.start.difficulty_id) == 0) {
          continue;
        }

        //correct encounter starttime

        std::chrono::milliseconds time =
          std::chrono::days{ 31 } *encounter.start_time.month
          + std::chrono::days{ encounter.start_time.day }
          + std::chrono::hours{ encounter.start_time.hour }
          + std::chrono::minutes{ encounter.start_time.minute }
          + std::chrono::seconds{ encounter.start_time.second }
        + std::chrono::milliseconds{ encounter.start_time.millisecond };

        sqlite3_exec(db_, "BEGIN;", nullptr, nullptr, nullptr);

        if (encounter_ids_.count(encounter.start.encounter_id) == 0) {
          prescience_helper::bind_sql(insert_encounter_type_stmt_, encounter.start.encounter_id, encounter.start.encounter_name);
          prescience_helper::exec(insert_encounter_type_stmt_, []() {});
          encounter_ids_.insert(encounter.start.encounter_id);
          encounter_name_to_id_.emplace(encounter.start.encounter_name, encounter.start.encounter_id);
          encounter_choice_->Append(to_wxString(encounter.start.encounter_name));
        } else { //if the encounter type didn't exist, the encounter can't have existed, so don't check
          prescience_helper::bind_sql(check_for_existing_encounter_stmt_,
            time.count(), encounter.start.encounter_id, (std::int32_t)encounter.start.difficulty_id, found_patch->id);
          std::int32_t count = 0;
          prescience_helper::exec<std::int32_t>(check_for_existing_encounter_stmt_, [&count](std::int32_t res) {
            count = res;
            });

          if (count != 0) {
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            continue;
          }
        }

        prescience_helper::bind_sql(insert_encounter_stmt_, encounter.start.encounter_id, found_patch->id, (std::int32_t)encounter.start.difficulty_id, time.count());
        prescience_helper::exec(insert_encounter_stmt_, []() {});

        const auto encounter_id = sqlite3_last_insert_rowid(db_);

        for (auto const& player : encounter.players) {
          if (player.second.damage.empty()) {
            //if player did no damage (e.g. reset or maybe a carry) ignore this pull
            continue;
          }
          prescience_helper::bind_sql(find_player_stmt_, player.first);
          std::optional<std::int32_t> player_id = std::nullopt;
          prescience_helper::exec<std::int32_t>(find_player_stmt_, [&player_id](std::int32_t id) {
            player_id = id;
            });
          if (!player_id.has_value()) {
            prescience_helper::bind_sql(insert_player_stmt_, player.first);
            prescience_helper::exec(insert_player_stmt_, []() {});
            player_id = sqlite3_last_insert_rowid(db_);
          }

          std::stringstream damage_string;
          for (std::size_t i = 0; i < player.second.damage.size(); ++i) {
            if (i > 0) {
              damage_string << ',';
            }
            damage_string << player.second.damage[i];
          }

          prescience_helper::bind_sql(insert_logged_stmt_, *player_id, (std::int32_t)player.second.info.current_spec_id, encounter_id, damage_string.view());
          prescience_helper::exec(insert_logged_stmt_, []() {});
        }
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
      }

      

      for (auto const& log : logs) {
        const auto path_string = log.path.string();
        prescience_helper::bind_sql(update_log_read_, path_string, (std::int64_t)log.to);
        prescience_helper::exec(update_log_read_, []() {});
      }

      set_output();
      std::stringstream output;
      output << "Read " << logs.size() << " log(s) and parsed " << parsed.size() << " encounter(s) at " << wxDateTime::Now().FormatTime().ToStdString();
      parse_info_text_->SetValue(output.str());
    }

    void on_input_change(wxCommandEvent&) {
      set_output();
    }

    void on_alternatives_per_time(wxSpinEvent&) {
      set_output();
    }

    void set_output() {
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

      std::vector<Time_info> time_info;
      std::vector<Raid_member> raid_members;

      const std::string input = input_text_->GetValue().ToStdString();
      std::vector<std::string_view> member_raw = clogparser::helpers::parse_array(input);

      if (member_raw.empty() || (member_raw.size() == 1 and member_raw.front() == "")) {
        gen_info_text_->SetValue("Invalid input");
        output_text_->SetValue("");
        return;
      }

      const auto alternatives_per_time = alternatives_per_time_->GetValue();

      raid_members.reserve(member_raw.size());

      std::vector<std::string_view> member_members;

      for (auto const& member : member_raw) {
        member_members.clear();
        clogparser::helpers::parse_array(member_members, member);

        if (member_members.size() != 2) {
          gen_info_text_->SetValue("Invalid member info, didn't have exactly 2 values");
          output_text_->SetValue("");
          return;
        }

        const auto guid = member_members[0];
        std::int32_t spec_id = -1;
        try {
          spec_id = clogparser::helpers::parseInt<std::int32_t>(member_members[1]);
        } catch (...) {
          gen_info_text_->SetValue("Couldn't parse spec id");
          output_text_->SetValue("");
          return;
        }

        raid_members.push_back(Raid_member{
          0,
          guid,
          spec_id
          });

        std::int32_t amounts_count = 0;
        time_info.clear();
        prescience_helper::bind_sql(get_logged_, guid, spec_id, found_encounter->second, found_difficulty->second);
        prescience_helper::exec<std::string_view>(get_logged_, [this, &amounts_count, &time_info](std::string_view amounts) {
          amounts_count++;

          const auto amount_vector = clogparser::helpers::parse_array(amounts);

          if (time_info.size() < amount_vector.size()) {
            time_info.resize(amount_vector.size());
          }

          for (std::size_t i = 0; i < amount_vector.size(); ++i) {
            try {
              time_info[i].amount += amounts_count * clogparser::helpers::parseInt<std::int64_t>(amount_vector[i]);
            } catch (...) {
              gen_info_text_->SetValue("Couldn't parse amount. This is an internal database problem, good luck");
              output_text_->SetValue("");
              return;
            }
            time_info[i].multiplier += amounts_count;
          }
          });

        raid_members.back().amounts.resize(time_info.size());

        for (std::size_t i = 0; i < time_info.size(); ++i) {
          raid_members.back().amounts[i] = ((double)time_info[i].amount) / time_info[i].multiplier;
          raid_members.back().average += raid_members.back().amounts[i];
        }
        if (time_info.size() != 0) {
          raid_members.back().average /= time_info.size();
        }
      }

      const std::size_t max_time = std::max_element(raid_members.begin(), raid_members.end(), [](Raid_member const& r1, Raid_member const& r2) {
        return r1.amounts.size() < r2.amounts.size();
        })->amounts.size();

        std::sort(raid_members.begin(), raid_members.end(), [](Raid_member const& r1, Raid_member const& r2) {
          return r1.average > r2.average;
          });

        for (std::size_t i = 0; i < raid_members.size() && i < alternatives_per_time; ++i) {
          raid_members[i].prio = alternatives_per_time - i;
        }

        for (std::size_t i = 0; i < max_time; ++i) {
          for (auto& raid_member : raid_members) {
            raid_member.amount_for_time = 0;
            for (std::size_t j = i; j < i + EXPECTED_EBON_MIGHT_UPTIME; ++j) {
              if (j < raid_member.amounts.size()) {
                raid_member.amount_for_time += raid_member.amounts[j];
              } else {
                raid_member.amount_for_time += raid_member.average;
              }
            }
          }
          std::sort(raid_members.begin(), raid_members.end(), [](Raid_member const& r1, Raid_member const& r2) {
            return r1.amount_for_time > r2.amount_for_time;
            });

          for (std::size_t i = 0; i < alternatives_per_time && i < raid_members.size(); ++i) {
            raid_members[i].prio += alternatives_per_time - i;
          }
        }

        std::stringstream sending;

        sending << "1?"; //version

        std::sort(raid_members.begin(), raid_members.end(), [](Raid_member const& r1, Raid_member const& r2) {
          return r1.prio > r2.prio;
          });

        for (auto const& raid_member : raid_members) {
          if (raid_member.prio == 0) {
            break;
          }

          sending <<
            raid_member.guid << '<' << (std::int64_t)raid_member.average / 100 << '<';
          for (auto const& by_time : raid_member.amounts) {
            sending << by_time / 100 << ',';
          }
          sending << '>';
        }

        output_text_->SetValue(sending.str());
        output_text_->SelectAll();
        output_text_->SetFocus();
        gen_info_text_->SetValue("Success at " + wxDateTime::Now().FormatTime());
    }

    ~Main_frame() {
      {
        std::lock_guard lock{ parse_thread_.transfer_mutex };
        parse_thread_.should_stop = true;
      }
      parse_thread_.thread.join();
    }

    enum CHILD_IDS : int {
      difficulties = wxID_HIGHEST + 1,
      encounters,
      log_location,
      log_finder_timer,
      input_text,
      alternatives_per_time,
    };
  private:
    wxDECLARE_EVENT_TABLE();

    std::vector<Patch> patches_;

    Parse_thread parse_thread_;
    sqlite3* db_;
    prescience_helper::Settings settings_;

    wxTextCtrl* log_location_text_;
    wxTimer log_finder_timer_;

    wxSpinCtrl* alternatives_per_time_;

    wxArrayString difficulties_;
    wxChoice* difficulty_choice_;
    std::unordered_map<std::string, std::int32_t> difficulty_to_id_;
    std::unordered_set<std::int32_t> difficulty_ids_;

    wxArrayString encounters_;
    std::unordered_set<std::int32_t> encounter_ids_;
    wxChoice* encounter_choice_;
    std::unordered_map<std::string, std::int32_t> encounter_name_to_id_;

    wxTextCtrl* input_text_;
    wxTextCtrl* output_text_;
    wxTextCtrl* gen_info_text_;
    wxTextCtrl* parse_info_text_;

    prescience_helper::Stmt insert_encounter_type_stmt_;
    prescience_helper::Stmt check_for_existing_encounter_stmt_;
    prescience_helper::Stmt insert_encounter_stmt_;
    prescience_helper::Stmt find_player_stmt_;
    prescience_helper::Stmt insert_player_stmt_;
    prescience_helper::Stmt insert_logged_stmt_;
    prescience_helper::Stmt update_log_read_;

    prescience_helper::Stmt get_logged_;
  };

  class Prescience_helper : public wxApp {
  public:
    bool OnInit() override {
      sqlite3* db = nullptr;
      if (sqlite3_open_v2("./prescience_helper.db", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        return false;
      }
      db_.reset(db);

      sqlite3_exec(db_.get(), "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

      if (sqlite3_exec(db_.get(), "SELECT COUNT(1) FROM Patch;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        sqlite3_exec(db_.get(), init_db.data(), nullptr, nullptr, nullptr);
      }

      Main_frame* frame = new Main_frame(db_.get());
      frame->Show();
      return true;
    }
  private:
    prescience_helper::Db db_;
  };
}

wxBEGIN_EVENT_TABLE(Main_frame, wxFrame)
  EVT_CHOICE(Main_frame::CHILD_IDS::difficulties, Main_frame::on_change_difficulty)
  EVT_CHOICE(Main_frame::CHILD_IDS::encounters, Main_frame::on_change_encounter)
  EVT_BUTTON(Main_frame::CHILD_IDS::log_location, Main_frame::on_press_change_log_location)
  EVT_TIMER(Main_frame::CHILD_IDS::log_finder_timer, Main_frame::on_log_finder_timer)
  EVT_TEXT(Main_frame::CHILD_IDS::input_text, Main_frame::on_input_change)
  EVT_SPINCTRL(Main_frame::CHILD_IDS::alternatives_per_time, Main_frame::on_alternatives_per_time)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP(Prescience_helper);