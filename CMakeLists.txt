cmake_minimum_required(VERSION 3.10)
project(jalalix C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(jalalix
  src/jalalix.c
  src/tz.c
  src/tzdata_embed.c
)

target_include_directories(jalalix PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(jalalix PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(jalalix PUBLIC m)

add_executable(test_jalalix tests/test_jalalix.c)
target_link_libraries(test_jalalix PRIVATE jalalix)

add_executable(basic_example examples/basic.c)
target_link_libraries(basic_example PRIVATE jalalix)

enable_testing()
add_test(NAME jalalix_smoke COMMAND test_jalalix)
