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
    GIT_TAG "main")

  FetchContent_MakeAvailable(clogparser)
endif()

add_library(prescience_helper_lib STATIC
  "src/prescience_helper_lib/ingest.cpp"
  "src/prescience_helper_lib/log_finder.cpp"
  "src/prescience_helper_lib/sim.cpp"
  "src/prescience_helper_lib/spells.cpp" 
  "src/prescience_helper_lib/dbc_constants.cpp"
  "src/prescience_helper_lib/specs/aug.cpp")

target_include_directories(prescience_helper_lib PRIVATE
  "include_private")

target_include_directories(prescience_helper_lib PUBLIC
  "include_public")

target_link_libraries(prescience_helper_lib PUBLIC 
  clogparser)

target_compile_features(prescience_helper_lib PUBLIC
  cxx_std_20)

add_executable(prescience_helper WIN32
  "src/prescience_helper/exe.cpp"
  "src/prescience_helper/sqlite3_wrapper.cpp")

target_link_libraries(prescience_helper PRIVATE
  prescience_helper_lib
  wx::core wx::base
  unofficial::sqlite3::sqlite3)

IF(${VCPKG_TARGET_TRIPLET} MATCHES ".*-static")
  set_property(TARGET prescience_helper_lib PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set_property(TARGET prescience_helper PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
ENDIF()