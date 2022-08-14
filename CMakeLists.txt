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
FetchContent_MakeAvailable(DPP pqxx)

find_package(spdlog REQUIRED)

set(SOURCES main.cpp
            src/database.cpp
            src/chocobot.cpp)
add_executable(chocobot ${SOURCES})

target_include_directories(chocobot PUBLIC include/)
target_link_libraries(chocobot PUBLIC dpp pqxx spdlog::spdlog)

set_target_properties(chocobot PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)
