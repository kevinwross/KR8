cmake_minimum_required(VERSION 3.25)

project(kr8 VERSION 0.2.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KR8_BUILD_TESTS "Build the KR8 test suite" ON)
set(KR8_UI_DEFAULT OFF)
if(WIN32 OR UNIX)
  set(KR8_UI_DEFAULT ON)
endif()
option(KR8_BUILD_UI "Build the windowed debugger UI" ${KR8_UI_DEFAULT})
option(KR8_SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
set(KR8_SDL3_SOURCE_DIR "" CACHE PATH
    "Optional SDL3 source tree; otherwise a pinned release is fetched")

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
  src/ui/source_editor.cpp
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

if(KR8_BUILD_UI)
  set(KR8_UI_SOURCES
    src/ui/app.cpp
    src/ui/help.cpp
    src/ui/syntax.cpp
    src/resources/embedded_fonts.cpp
  )
  if(WIN32)
    list(APPEND KR8_UI_SOURCES
      src/platform/ui/win32/run_win32.cpp
      src/platform/ui/win32/win32_canvas.cpp
      src/platform/ui/win32/win32_window.cpp
      src/platform/ui/win32/win32_launcher.cpp
    )
  elseif(UNIX)
    list(APPEND KR8_UI_SOURCES
      src/platform/ui/sdl/run_sdl.cpp
      src/platform/ui/sdl/sdl_canvas.cpp
      src/platform/ui/sdl/sdl_window.cpp
    )
  endif()

  add_library(kr8_ui STATIC ${KR8_UI_SOURCES})
  target_include_directories(kr8_ui PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
  )
  target_link_libraries(kr8_ui PUBLIC kr8_core)

  if(WIN32)
    target_compile_definitions(kr8_ui PRIVATE
      UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS
    )
    target_link_libraries(kr8_ui PRIVATE user32 gdi32 comdlg32 shell32)
  elseif(UNIX)
    target_include_directories(kr8_ui PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
    )
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
    set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
    set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
    set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
    set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
    if(KR8_SDL3_SOURCE_DIR)
      add_subdirectory("${KR8_SDL3_SOURCE_DIR}"
                       "${CMAKE_BINARY_DIR}/sdl3")
    else()
      FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG 535d80badefc83c5c527ec5748f2a20d6a9310fe
        GIT_SHALLOW FALSE
      )
      FetchContent_MakeAvailable(SDL3)
    endif()
    foreach(sdl_target SDL_uclibc SDL3-static SDL3_test)
      if(TARGET ${sdl_target})
        target_compile_options(${sdl_target} PRIVATE -Wno-error)
      endif()
    endforeach()
    target_compile_definitions(kr8_ui PRIVATE SDL_MAIN_HANDLED)
    if(TARGET SDL3::SDL3-static)
      target_link_libraries(kr8_ui PRIVATE SDL3::SDL3-static)
    else()
      target_link_libraries(kr8_ui PRIVATE SDL3::SDL3)
    endif()
    target_link_libraries(kr8_ui PRIVATE dl pthread m)
  endif()

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
