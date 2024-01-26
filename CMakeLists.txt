cmake_minimum_required(VERSION 3.8)

project(prescience_helper CXX)

find_package(wxWidgets CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(
  clogparser
  GIT_REPOSITORY "https://github.com/dekibeki/clogparser.git"
  GIT_TAG "main")

FetchContent_MakeAvailable(clogparser)

add_library(prescience_helper_lib STATIC
  "src/ingest.cpp"
  "src/log_finder.cpp")

target_include_directories(prescience_helper_lib PUBLIC
  "include_public")

target_link_libraries(prescience_helper_lib PUBLIC 
  clogparser)

target_compile_features(prescience_helper_lib PUBLIC
  cxx_std_20)

IF(${VCPKG_TARGET_TRIPLET} MATCHES ".*-static")
  set_property(TARGET prescience_helper_lib PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
ENDIF()

add_executable(prescience_helper WIN32
  "src/exe.cpp")

target_link_libraries(prescience_helper PRIVATE
  prescience_helper_lib
  wx::core wx::base
  unofficial::sqlite3::sqlite3)
IF(${VCPKG_TARGET_TRIPLET} MATCHES ".*-static")
  set_property(TARGET prescience_helper_embedded PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
ENDIF()