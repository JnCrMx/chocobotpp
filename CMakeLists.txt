cmake_minimum_required(VERSION 3.10)

project(chocobotpp LANGUAGES CXX VERSION 1.0.0 DESCRIPTION "A simple Discord Bot")

include(FetchContent)

function(FetchContent_MakeAvailable_ExcludeFromAll)
  if(CMAKE_VERSION VERSION_LESS "3.28.0")
    foreach(_name ${ARGN})
        FetchContent_GetProperties(${_name})
        if(NOT ${_name}_POPULATED)
            FetchContent_Populate(${_name})
            add_subdirectory(${${_name}_SOURCE_DIR} ${${_name}_BINARY_DIR} EXCLUDE_FROM_ALL)
        endif()
    endforeach()
  else()
    FetchContent_MakeAvailable(${ARGN})
  endif()
endfunction()

FetchContent_Declare(DPP
  GIT_REPOSITORY https://github.com/brainboxdotcc/DPP.git
  GIT_TAG        v10.0.30
  EXCLUDE_FROM_ALL
)
FetchContent_Declare(pqxx
  GIT_REPOSITORY https://github.com/jtv/libpqxx.git
  GIT_TAG        7.9.2
  EXCLUDE_FROM_ALL
)
FetchContent_Declare(date
  GIT_REPOSITORY https://github.com/HowardHinnant/date.git
  GIT_TAG        v3.0.1
  EXCLUDE_FROM_ALL
)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.0
  EXCLUDE_FROM_ALL
)
FetchContent_Declare(cmake_git_version_tracking
  GIT_REPOSITORY https://github.com/andrew-hardin/cmake-git-version-tracking.git
  GIT_TAG 6c0cb87edd029ddfb403a8e24577c144a03605a6
  EXCLUDE_FROM_ALL
)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz EXCLUDE_FROM_ALL)
FetchContent_Declare(pistache
  GIT_REPOSITORY https://github.com/pistacheio/pistache.git
  GIT_TAG        master
  EXCLUDE_FROM_ALL
)

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
set(PISTACHE_BUILD_TESTS OFF CACHE BOOL "Pistache: Build tests" FORCE)

option(USE_INSTALLED_DPP "Use preinstalled DPP" OFF)
option(USE_INSTALLED_PQXX "Use preinstalled pxqq" OFF)

FetchContent_MakeAvailable_ExcludeFromAll(date cmake_git_version_tracking pistache)
if(BUILD_TEST)
  FetchContent_MakeAvailable_ExcludeFromAll(Catch2)
endif(BUILD_TEST)

if(USE_INSTALLED_DPP)
  find_package(dpp REQUIRED)
  FetchContent_MakeAvailable_ExcludeFromAll(json)
else()
  FetchContent_MakeAvailable_ExcludeFromAll(DPP)
  add_library(dpp::dpp ALIAS dpp)
endif(USE_INSTALLED_DPP)

if(USE_INSTALLED_PQXX)
  find_package(libpqxx REQUIRED)
else()
  FetchContent_MakeAvailable_ExcludeFromAll(pqxx)
  add_library(libpqxx::pqxx ALIAS pqxx)
endif(USE_INSTALLED_PQXX)

find_package(spdlog REQUIRED)
find_package(ImageMagick COMPONENTS Magick++ MagickCore REQUIRED)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)
add_library(spdlogModule STATIC)
target_sources(spdlogModule PUBLIC FILE_SET CXX_MODULES FILES third_party/spdlog.cppm)
target_compile_features(spdlogModule PUBLIC cxx_std_23)
target_link_libraries(spdlogModule PUBLIC spdlog::spdlog)

add_library(pistacheModule STATIC)
target_sources(pistacheModule PUBLIC FILE_SET CXX_MODULES FILES third_party/pistache.cppm)
target_compile_features(pistacheModule PUBLIC cxx_std_23)
target_link_libraries(pistacheModule PUBLIC pistache)

add_library(pqxxModule STATIC)
target_sources(pqxxModule PUBLIC FILE_SET CXX_MODULES FILES third_party/pqxx.cppm)
target_compile_features(pqxxModule PUBLIC cxx_std_23)
target_link_libraries(pqxxModule PUBLIC libpqxx::pqxx)

add_library(dppModule STATIC)
target_sources(dppModule PUBLIC FILE_SET CXX_MODULES FILES third_party/dpp.cppm)
target_compile_features(dppModule PUBLIC cxx_std_23)
target_link_libraries(dppModule PUBLIC dpp::dpp)
if(USE_INSTALLED_DPP) # DPP doesn't properly export include directories nor libraries
  target_include_directories(dppModule PUBLIC ${dpp_DIR}/../../../include)
  target_link_libraries(dppModule PUBLIC nlohmann_json::nlohmann_json)
  target_link_libraries(dppModule PUBLIC crypto ssl)
endif(USE_INSTALLED_DPP)

set(SOURCES
  main.cpp
  src/api.cpp
  src/database.cpp
  src/chocobot.cpp
  src/utils.cpp
  src/paid_command.cpp
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
  src/commands/shop.cpp
  src/commands/fortune.cpp
  src/commands/magick.cpp
  src/commands/merry_christmas.cpp
  src/commands/christmas.cpp
  src/commands/purge.cpp
  src/games/slot-machine.cpp
  src/games/who-is-it.cpp
  src/games/quiz.cpp
)
set(MODULE_SOURCES
  src/chocobot.cppm
  src/branding.cppm
  src/config.cppm
  src/database.cppm
  src/i18n.cppm
  src/utils.cppm
)
add_executable(chocobot ${SOURCES})
target_sources(chocobot PUBLIC
  FILE_SET CXX_MODULES
  BASE_DIRS src
  FILES ${MODULE_SOURCES}
)

target_include_directories(chocobot PUBLIC include/)

target_include_directories(chocobot PUBLIC
  ${ImageMagick_INCLUDE_DIRS}
)
target_link_libraries(chocobot PUBLIC
  dppModule pqxxModule spdlogModule pistacheModule
  ${ImageMagick_LIBRARIES}
  date::date date::date-tz
  cmake_git_version_tracking
  ssl
)
target_compile_definitions(chocobot PUBLIC -DMAGICKCORE_QUANTUM_DEPTH=16 )
target_compile_definitions(chocobot PUBLIC -DMAGICKCORE_HDRI_ENABLE=0 )

target_compile_features(chocobot PUBLIC cxx_std_23)

add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/resources
)
add_dependencies(chocobot copy_assets)

install(TARGETS chocobot)
install(DIRECTORY resources DESTINATION share/chocobot)

if(BUILD_TEST)
  add_subdirectory(test)
endif(BUILD_TEST)
