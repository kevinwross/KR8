add_executable(kr8_tests
  main.cpp
  test_smoke.cpp
  test_isa.cpp
  test_image.cpp
  test_cpu_decode.cpp
  test_cpu_alu.cpp
  test_cpu_control.cpp
  test_cpu_io.cpp
  test_machine.cpp
)

target_include_directories(kr8_tests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(kr8_tests PRIVATE kr8_core)

add_test(NAME kr8.tests COMMAND kr8_tests)
