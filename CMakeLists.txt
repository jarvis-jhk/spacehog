cmake_minimum_required(VERSION 3.20)
project(spacehog C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(SDL3 REQUIRED CONFIG)

file(GLOB COMPAT_SRC src/compat/*.c)
file(GLOB GAME_SRC src/game/*.c)
add_executable(spacehog src/main.c src/platform/sys.c ${COMPAT_SRC} ${GAME_SRC} src/third_party/nuked_opl3/opl3.c)
target_include_directories(spacehog PRIVATE src)
target_link_libraries(spacehog PRIVATE SDL3::SDL3-static)
if(MSVC)
  target_compile_options(spacehog PRIVATE /W3)
else()
  # the original was built by gcc 2.x for i386: keep signed overflow wrapping and
  # plain char signedness explicit so translated arithmetic behaves the same
  target_compile_options(spacehog PRIVATE -Wall -Wextra -Wno-unused-parameter -fwrapv -fsigned-char)
endif()
if(WIN32)
  set_target_properties(spacehog PROPERTIES WIN32_EXECUTABLE ON)
  target_link_options(spacehog PRIVATE -static)
endif()
