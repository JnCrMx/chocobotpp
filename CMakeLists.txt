cmake_minimum_required(VERSION 3.10)

project(chocobotpp)

include(FetchContent)
FetchContent_Declare(DPP
  GIT_REPOSITORY https://github.com/brainboxdotcc/DPP.git
  GIT_TAG        v10.0.23
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

set(BUILD_TZ_LIB ON CACHE BOOL "build/install of TZ library" FORCE)
set(USE_SYSTEM_TZ_DB ON CACHE BOOL "Use the operating system's timezone database" FORCE)
set(SKIP_BUILD_TEST ON CACHE BOOL "Skips the test option for subprojects" FORCE)

option(BUILD_TEST "Build the tests" OFF)
if(NOT BUILD_TEST)
  set(DPP_BUILD_TEST OFF CACHE BOOL "Build the test program" FORCE)
endif(NOT BUILD_TEST)

FetchContent_MakeAvailable(DPP pqxx date cmake_git_version_tracking)
if(BUILD_TEST)
  FetchContent_MakeAvailable(Catch2)
endif(BUILD_TEST)

find_package(spdlog REQUIRED)
find_package(ImageMagick COMPONENTS Magick++ MagickCore REQUIRED)

set(SOURCES main.cpp
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
add_executable(chocobot ${SOURCES})

target_include_directories(chocobot PUBLIC include/)

target_include_directories(chocobot PUBLIC
  ${ImageMagick_INCLUDE_DIRS}
)
target_link_libraries(chocobot PUBLIC
  dpp pqxx spdlog::spdlog
  ${ImageMagick_LIBRARIES}
  date::date date::date-tz
  cmake_git_version_tracking
)
target_compile_definitions(chocobot PUBLIC -DMAGICKCORE_QUANTUM_DEPTH=16 )
target_compile_definitions(chocobot PUBLIC -DMAGICKCORE_HDRI_ENABLE=0 )

set_target_properties(chocobot PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
)

add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/resources
)
add_dependencies(chocobot copy_assets)

install(TARGETS chocobot DESTINATION bin)
install(DIRECTORY resources DESTINATION share/chocobot)

if(BUILD_TEST)
  add_subdirectory(test)
endif(BUILD_TEST)
