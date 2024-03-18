cmake_minimum_required(VERSION 3.15)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW) 
endif()

project(prescience_helper CXX)

find_package(wxWidgets CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

find_package(clogparser)

if(NOT ${clogparser_FOUND})
  include(FetchContent)
  FetchContent_Declare(
    clogparser
    GIT_REPOSITORY "https://github.com/dekibeki/clogparser.git"
    GIT_TAG "cbc7eeb2c23c5bf7556243679acd397bb8f42c96")

  FetchContent_MakeAvailable(clogparser)
endif()

add_library(csv_to_include STATIC
  "csv_to_include/src/csv_to_include.cpp")

target_include_directories(csv_to_include PUBLIC
  "csv_to_include/include_private")

target_compile_features(csv_to_include PUBLIC
  cxx_std_20)

add_executable(csv_to_randproppoints
  "csv_to_randproppoints/src/csv_to_randproppoints.cpp")

target_link_libraries(csv_to_randproppoints PRIVATE
  csv_to_include)

add_executable(csv_to_itemsparse
  "csv_to_itemsparse/src/csv_to_itemsparse.cpp")

target_link_libraries(csv_to_itemsparse PRIVATE
  csv_to_include)

add_executable(csv_to_combatratingsmultbyilvl
  "csv_to_combatratingsmultbyilvl/src/csv_to_combatratingsmultbyilvl.cpp")

target_link_libraries(csv_to_combatratingsmultbyilvl PRIVATE
  csv_to_include)

add_executable(csv_to_spelleffect
  "csv_to_spelleffect/src/csv_to_spelleffect.cpp")

target_link_libraries(csv_to_spelleffect PRIVATE
  csv_to_include)

add_executable(csv_to_spellmisc
  "csv_to_spellmisc/src/csv_to_spellmisc.cpp")

target_link_libraries(csv_to_spellmisc PRIVATE
  csv_to_include
  clogparser)

add_library(prescience_helper_lib STATIC
  "prescience_helper_lib/src/ingest.cpp"
  "prescience_helper_lib/src/sim.cpp"
  "prescience_helper_lib/src/spells.cpp" 
  "prescience_helper_lib/src/dbc_constants.cpp"
  "prescience_helper_lib/src/specs/aug.cpp"
  "prescience_helper_lib/src/dbc/rand_prop_points.cpp"
  "prescience_helper_lib/src/dbc/item_sparse.cpp"
  "prescience_helper_lib/src/dbc/combat_ratings_mult_by_ilvl.cpp"
  "prescience_helper_lib/src/dbc/spell_effect.cpp"
  "prescience_helper_lib/src/helpers.cpp"
  "prescience_helper_lib/src/on_rails.cpp"
  "prescience_helper_lib/src/dbc/spell_misc.cpp")

target_include_directories(prescience_helper_lib PRIVATE
  "prescience_helper_lib/include_private")

target_include_directories(prescience_helper_lib PUBLIC
  "prescience_helper_lib/include_public")

target_link_libraries(prescience_helper_lib PUBLIC 
  clogparser)

target_compile_features(prescience_helper_lib PUBLIC
  cxx_std_20)

add_executable(prescience_helper WIN32
  "prescience_helper/src/exe.cpp"
  "prescience_helper/src/log_finder.cpp"
  "prescience_helper/src/sqlite3_wrapper.cpp"
  "prescience_helper/src/serialize.cpp")

target_include_directories(prescience_helper PRIVATE
  "prescience_helper/include_private")

target_link_libraries(prescience_helper PRIVATE
  prescience_helper_lib
  wx::core wx::base
  unofficial::sqlite3::sqlite3)

add_executable(scratch
  "scratch/src/scratch.cpp")

target_link_libraries(scratch PRIVATE
  prescience_helper_lib)

IF(${VCPKG_TARGET_TRIPLET} MATCHES ".*-static")
  set_property(TARGET prescience_helper_lib PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set_property(TARGET prescience_helper PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set_property(TARGET scratch PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
ENDIF()