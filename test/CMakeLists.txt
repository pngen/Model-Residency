# Model Residency test suite. There is deliberately no CTest timeout
# anywhere: tests run to natural completion, pass, fail, or crash, and are
# manually terminated only after a genuine hang is diagnosed.

set(MR_TEST_SOURCES
  mr_test_main.cpp
  test_core.cpp
  test_coordinator.cpp
)

add_executable(mr_tests ${MR_TEST_SOURCES})
target_link_libraries(mr_tests PRIVATE mr_core)
target_include_directories(mr_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME mr_core_suite COMMAND mr_tests)
