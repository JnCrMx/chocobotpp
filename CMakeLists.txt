cmake_minimum_required(VERSION 3.10)

project(chocobotpp)

include(FetchContent)
FetchContent_Declare(DPP
  GIT_REPOSITORY https://github.com/brainboxdotcc/DPP.git
  GIT_TAG        v10.0.15
)
FetchContent_MakeAvailable(DPP)

set(SOURCES chocobot.cpp)
add_executable(chocobot ${SOURCES})

target_link_libraries(chocobot PUBLIC dpp)
