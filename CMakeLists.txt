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

find_package(GTest)

if(GTEST_FOUND)
  add_executable(unit_tests ${sources_test} ${sources})

  target_compile_definitions(unit_tests PUBLIC UNIT_TESTS)

  set_target_properties(example PROPERTIES ENABLE_EXPORTS on)

  target_link_libraries(unit_tests PUBLIC
    ${GTEST_BOTH_LIBRARIES}
    kopchik
  )

  target_include_directories(unit_tests PUBLIC
    ${GTEST_INCLUDE_DIRS}
  )
endif()
