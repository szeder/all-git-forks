cmake_minimum_required(VERSION 3.6)

add_custom_target(git COMMAND cd ${PROJECT_SOURCE_DIR} && make)

file(GLOB_RECURSE SRCS *.c)
add_executable(z_dummy_target ${SRCS})

include_directories(${PROJECT_SOURCE_DIR})
