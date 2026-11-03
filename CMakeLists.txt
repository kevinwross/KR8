cmake_minimum_required(VERSION 3.25)

project(kr8 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KR8_BUILD_TESTS "Build the KR8 test suite" ON)
set(KR8_UI_DEFAULT OFF)
if(WIN32)
  set(KR8_UI_DEFAULT ON)
endif()
option(KR8_BUILD_UI "Build the windowed debugger UI" ${KR8_UI_DEFAULT})
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

if(KR8_BUILD_UI AND WIN32)
  add_library(kr8_ui STATIC
    src/ui/app.cpp
    src/ui/help.cpp
    src/ui/text_field.cpp
    src/ui/run_win32.cpp
    src/platform/ui/win32/win32_canvas.cpp
    src/platform/ui/win32/win32_window.cpp
    src/platform/ui/win32/win32_launcher.cpp
    src/resources/embedded_fonts.cpp
  )
  target_include_directories(kr8_ui PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_compile_definitions(kr8_ui PRIVATE
    UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
  )
  target_link_libraries(kr8_ui PUBLIC kr8_core)
  target_link_libraries(kr8_ui PRIVATE user32 gdi32 comdlg32 shell32)

  add_executable(kr8_ui_smoke src/ui/main.cpp)
  target_link_libraries(kr8_ui_smoke PRIVATE kr8_ui)

  target_compile_definitions(kr8 PRIVATE KR8_HAS_UI=1)
  target_include_directories(kr8 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  target_link_libraries(kr8 PRIVATE kr8_ui)
endif()

if(KR8_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
