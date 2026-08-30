set(MR_CORE_SOURCES
  core/uid.cpp
  core/enums.cpp
  core/byte_size.cpp
  core/json.cpp
  model.cpp
  compatibility.cpp
  capacity.cpp
  residency.cpp
  authority.cpp
  explain.cpp
  placement.cpp
  coordinator.cpp
  persistence.cpp
  backends/memory_backend.cpp
)

add_library(mr_core STATIC ${MR_CORE_SOURCES})

target_include_directories(mr_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(mr_core PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(MSVC)
  target_compile_options(mr_core PRIVATE /wd4251 /wd4275)
endif()
