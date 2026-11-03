cmake_minimum_required(VERSION 3.25)

project(kr8 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KR8_BUILD_TESTS "Build the KR8 test suite" ON)
option(KR8_BUILD_UI "Build the windowed debugger UI (enabled in M4)" OFF)
option(KR8_SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_compile_options(/W4 /WX /permissive- /Zc:__cplusplus /Brepro)
  add_link_options(/Brepro)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
  if(KR8_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

add_library(kr8_core STATIC
  src/core/version.cpp
  src/isa/isa.cpp
  src/image/image.cpp
  src/asm/assembler.cpp
  src/dis/disassembler.cpp
  src/debug/session.cpp
  src/vm/cpu.cpp
  src/vm/machine.cpp
  src/vm/runner.cpp
  src/platform/console.cpp
)

target_include_directories(kr8_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_definitions(kr8_core PUBLIC
  KR8_VERSION="${PROJECT_VERSION}"
)

add_executable(kr8
  src/cli/main.cpp
)
target_link_libraries(kr8 PRIVATE kr8_core)

if(KR8_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
