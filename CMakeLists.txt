cmake_minimum_required(VERSION 3.10)

project(chocobotpp)

include(FetchContent)
FetchContent_Declare(DPP
  GIT_REPOSITORY https://github.com/brainboxdotcc/DPP.git
  GIT_TAG        v10.0.15
)
FetchContent_Declare(pqxx
  GIT_REPOSITORY https://github.com/jtv/libpqxx.git
  GIT_TAG        7.7.3
)
FetchContent_Declare(date
  GIT_REPOSITORY https://github.com/HowardHinnant/date.git
  GIT_TAG        v3.0.1
)

set(BUILD_TZ_LIB ON CACHE INTERNAL BOOL "build/install of TZ library")
set(USE_SYSTEM_TZ_DB ON CACHE INTERNAL BOOL "Use the operating system's timezone database")

FetchContent_MakeAvailable(DPP pqxx date)

find_package(spdlog REQUIRED)

set(SOURCES main.cpp
            src/database.cpp
            src/chocobot.cpp
            src/i18n.cpp
            src/utils.cpp
            src/commands/ping.cpp
            src/commands/coins.cpp
            src/commands/ship.cpp
            src/commands/daily.cpp
            src/commands/remind.cpp)
add_executable(chocobot ${SOURCES})

target_include_directories(chocobot PUBLIC include/)
target_link_libraries(chocobot PUBLIC dpp pqxx spdlog::spdlog date::date date::date-tz)

set_target_properties(chocobot PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
)

target_link_libraries(chocobot PRIVATE -fsanitize=address)

add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/resources
)
add_dependencies(chocobot copy_assets)
