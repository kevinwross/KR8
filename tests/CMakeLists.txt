add_executable(kr8_tests
  main.cpp
  test_smoke.cpp
  test_isa.cpp
  test_image.cpp
  test_assembler.cpp
  test_disassembler.cpp
  test_examples.cpp
  test_runner.cpp
  test_session_load.cpp
  test_session_steps.cpp
  test_session_breakpoints.cpp
  test_session_io.cpp
  test_cpu_decode.cpp
  test_cpu_alu.cpp
  test_cpu_control.cpp
  test_cpu_io.cpp
  test_machine.cpp
  test_source_editor.cpp
)

target_include_directories(kr8_tests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(kr8_tests PRIVATE
  KR8_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../examples"
)
target_link_libraries(kr8_tests PRIVATE kr8_core)

add_test(NAME kr8.tests COMMAND kr8_tests)

add_test(NAME kr8.cli.version COMMAND kr8 --version)
set_tests_properties(kr8.cli.version PROPERTIES
  PASS_REGULAR_EXPRESSION "KR8 0.2.2"
)

add_test(NAME kr8.cli.as
  COMMAND kr8 as "${CMAKE_CURRENT_SOURCE_DIR}/../examples/hello.asm"
          -o "${CMAKE_CURRENT_BINARY_DIR}/hello.k8" -q
)
set_tests_properties(kr8.cli.as PROPERTIES
  PASS_REGULAR_EXPRESSION ""
)

add_test(NAME kr8.cli.dis
  COMMAND kr8 dis "${CMAKE_CURRENT_BINARY_DIR}/hello.k8" --symbols
)
set_tests_properties(kr8.cli.dis PROPERTIES
  DEPENDS kr8.cli.as
  PASS_REGULAR_EXPRESSION "JMP loop"
)

add_test(NAME kr8.cli.sym
  COMMAND kr8 sym "${CMAKE_CURRENT_BINARY_DIR}/hello.k8"
)
set_tests_properties(kr8.cli.sym PROPERTIES
  DEPENDS kr8.cli.as
  PASS_REGULAR_EXPRESSION "msg"
)

add_test(NAME kr8.cli.run
  COMMAND kr8 run "${CMAKE_CURRENT_BINARY_DIR}/hello.k8"
)
set_tests_properties(kr8.cli.run PROPERTIES
  DEPENDS kr8.cli.as
  PASS_REGULAR_EXPRESSION "Hello, KR8!"
)

if(TARGET kr8_ui_smoke AND UNIX)
  find_program(KR8_XVFB_RUN xvfb-run)
  find_program(KR8_XDOTOOL xdotool)
  find_program(KR8_IMPORT import)
  if(KR8_XVFB_RUN AND KR8_XDOTOOL AND KR8_IMPORT)
    add_test(NAME kr8.ui.smoke.linux
      COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/ui_smoke_linux.sh"
              "$<TARGET_FILE:kr8_ui_smoke>"
              "${CMAKE_CURRENT_BINARY_DIR}/hello.k8"
              "${CMAKE_CURRENT_BINARY_DIR}/ui-smoke.png"
    )
    set_tests_properties(kr8.ui.smoke.linux PROPERTIES
      DEPENDS kr8.cli.as
      LABELS "ui"
      TIMEOUT 20
    )
  endif()
endif()

if(UNIX)
  add_test(NAME kr8.platform.boundary
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/platform_boundary.sh"
  )
  set_tests_properties(kr8.platform.boundary PROPERTIES LABELS "boundary")
endif()
