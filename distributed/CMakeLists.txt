# Framed-TCP distributed coordinator + worker processes and the atomic
# multiprocess restart proof.

add_library(mr_distributed STATIC
  ${CMAKE_SOURCE_DIR}/src/net.cpp
  ${CMAKE_SOURCE_DIR}/src/protocol.cpp
)
target_include_directories(mr_distributed PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(mr_distributed PUBLIC mr_core)
if(WIN32)
  target_link_libraries(mr_distributed PUBLIC ws2_32)
endif()

add_executable(mr_coordinator coordinator_main.cpp)
target_link_libraries(mr_coordinator PRIVATE mr_core mr_distributed)

add_executable(mr_worker worker_main.cpp)
target_link_libraries(mr_worker PRIVATE mr_core mr_distributed)

if(MR_BUILD_TESTS)
  add_executable(multiprocess_proof multiprocess_proof.cpp)
  target_link_libraries(multiprocess_proof PRIVATE mr_core mr_distributed)
  add_test(NAME mr_multiprocess_restart
           COMMAND multiprocess_proof $<TARGET_FILE:mr_coordinator> $<TARGET_FILE:mr_worker>)
endif()
