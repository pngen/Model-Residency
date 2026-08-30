foreach(ex basic_residency model_shards adapter_residency persistence_recovery)
  add_executable(${ex} ${ex}.cpp)
  target_link_libraries(${ex} PRIVATE mr_core)
endforeach()
