cmake_minimum_required(VERSION 3.10)

project(chocobotpp)

include(FetchContent)
FetchContent_Declare(DPP
  GIT_REPOSITORY https://github.com/brainboxdotcc/DPP.git
  GIT_TAG        v10.0.26
)
FetchContent_Declare(pqxx
  GIT_REPOSITORY https://github.com/jtv/libpqxx.git
  GIT_TAG        7.7.4
)
FetchContent_Declare(date
  GIT_REPOSITORY https://github.com/HowardHinnant/date.git
  GIT_TAG        v3.0.1
)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.1.0
)
FetchContent_Declare(cmake_git_version_tracking
  GIT_REPOSITORY https://github.com/andrew-hardin/cmake-git-version-tracking.git
  GIT_TAG 7d9442bfade5a47b77291e4117f81cc65887c7b9
)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_Declare(Simple-Web-Server
  GIT_REPOSITORY https://gitlab.com/eidheim/Simple-Web-Server.git
  GIT_TAG        27b41f5ee154cca0fce4fe2955dd886d04e3a4ed)

set(BUILD_TZ_LIB ON CACHE BOOL "date: build/install of TZ library" FORCE)
set(USE_SYSTEM_TZ_DB ON CACHE BOOL "date: Use the operating system's timezone database" FORCE)
set(SKIP_BUILD_TEST ON CACHE BOOL "Skips the test option for subprojects" FORCE)

option(BUILD_TEST "Build the tests" OFF)
if(NOT BUILD_TEST)
  set(DPP_BUILD_TEST OFF CACHE BOOL "DPP: Build the test program" FORCE)
endif(NOT BUILD_TEST)
set(DPP_CORO ON CACHE BOOL "DPP: Experimental support for C++20 coroutines" FORCE)
set(BUILD_VOICE_SUPPORT OFF CACHE BOOL "DPP: Build voice support" FORCE)
set(RUN_LDCONFIG OFF CACHE BOOL "DPP: Run ldconfig after installation" FORCE)
set(USE_OPENSSL OFF CACHE BOOL "Simple-Web-Server: set OFF to build without OpenSSL" FORCE)

option(USE_INSTALLED_DPP "Use preinstalled DPP" OFF)
option(USE_INSTALLED_PQXX "Use preinstalled pxqq" OFF)

FetchContent_MakeAvailable(date cmake_git_version_tracking Simple-Web-Server)
if(BUILD_TEST)
  FetchContent_MakeAvailable(Catch2)
endif(BUILD_TEST)

if(USE_INSTALLED_DPP)
  find_package(dpp REQUIRED)
  FetchContent_MakeAvailable(json)
else()
  FetchContent_MakeAvailable(DPP)
  add_library(dpp::dpp ALIAS dpp)
endif(USE_INSTALLED_DPP)

if(USE_INSTALLED_PQXX)
  find_package(libpqxx REQUIRED)
else()
  FetchContent_MakeAvailable(pqxx)
  add_library(libpqxx::pqxx ALIAS pqxx)
endif(USE_INSTALLED_PQXX)

find_package(spdlog REQUIRED)
find_package(ImageMagick COMPONENTS Magick++ MagickCore REQUIRED)

set(SOURCES main.cpp
            src/api.cpp
            src/database.cpp
            src/chocobot.cpp
            src/i18n.cpp
            src/utils.cpp
            src/commands/ping.cpp
            src/commands/coins.cpp
            src/commands/ship.cpp
            src/commands/daily.cpp
            src/commands/remind.cpp
            src/commands/gift.cpp
            src/commands/noop.cpp
            src/commands/random.cpp
            src/commands/credits.cpp
            src/commands/bugreport.cpp
            src/commands/help.cpp
            src/commands/mystats.cpp
            src/games/slot-machine.cpp
            src/games/who-is-it.cpp
            src/games/quiz.cpp)
set(HEADERS "include/chocobot.hpp"
            "include/database.hpp"
            "include/api.hpp"
            "include/command.hpp"
            "include/games.hpp"
            "include/i18n.hpp"
            "include/utils.hpp"
            "include/branding.hpp"
            "include/config.hpp")
add_executable(chocobot ${SOURCES})
#target_precompile_headers(chocobot PRIVATE ${HEADERS})

target_include_directories(chocobot PUBLIC include/)

target_include_directories(chocobot PUBLIC
  ${ImageMagick_INCLUDE_DIRS}
)
target_link_libraries(chocobot PUBLIC
  dpp::dpp libpqxx::pqxx spdlog::spdlog
  ${ImageMagick_LIBRARIES}
  date::date date::date-tz
  cmake_git_version_tracking
  simple-web-server
  ssl
)
target_compile_definitions(chocobot PUBLIC -DMAGICKCORE_QUANTUM_DEPTH=16 )
target_compile_definitions(chocobot PUBLIC -DMAGICKCORE_HDRI_ENABLE=0 )

if(USE_INSTALLED_DPP) # DPP doesn't properly export include directories nor libraries
  target_include_directories(chocobot PUBLIC ${dpp_DIR}/../../../include)
  target_link_libraries(chocobot PUBLIC nlohmann_json::nlohmann_json)
  target_link_libraries(chocobot PUBLIC crypto ssl)
endif(USE_INSTALLED_DPP)

set_target_properties(chocobot PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
)
target_compile_features(chocobot PUBLIC cxx_std_23)

add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/resources
)
add_dependencies(chocobot copy_assets)

install(TARGETS chocobot DESTINATION bin)
install(DIRECTORY resources DESTINATION share/chocobot)

if(BUILD_TEST)
  add_subdirectory(test)
endif(BUILD_TEST)
