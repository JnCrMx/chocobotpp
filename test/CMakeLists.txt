set(command_tests ship)

set(command_tests_commands ${command_tests})
set(command_tests_tests ${command_tests})
list(TRANSFORM command_tests_commands PREPEND ../src/commands/)
list(TRANSFORM command_tests_commands APPEND .cpp)
list(TRANSFORM command_tests_tests PREPEND commands/)
list(TRANSFORM command_tests_tests APPEND .cpp)
message("Commands to test: " ${command_tests} " => " ${command_tests_commands} " " ${command_tests_tests})
add_executable(command_tests
    command_test.cpp
    ../src/utils.cpp ../src/i18n.cpp
    ${command_tests_commands} ${command_tests_tests})
target_link_libraries(command_tests PRIVATE Catch2::Catch2WithMain)
target_link_libraries(command_tests PRIVATE dpp pqxx spdlog::spdlog date::date date::date-tz)
target_include_directories(command_tests PRIVATE ../include)
set_target_properties(command_tests PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(command_tests)
