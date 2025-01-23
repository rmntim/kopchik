cmake_minimum_required(VERSION 3.0)
project(kopchik)

file(GLOB_RECURSE sources      src/*.c src/*.h)
file(GLOB_RECURSE sources_test tests/*.c)

add_executable(kopchik ${sources})

target_compile_options(kopchik PUBLIC -std=c99 -Wall -Wextra -pedantic -Wfloat-conversion)
target_compile_definitions(kopchik
    PUBLIC
      $<$<CONFIG:Debug>:KOP_DEBUG>
)
