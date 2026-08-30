cmake_minimum_required(VERSION 3.24)

project(ModelResidency VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MR_ENABLE_CUDA "Enable CUDA residency validation backend" ON)
option(MR_BUILD_SHARED "Build shared library instead of static" OFF)
option(MR_BUILD_TESTS "Build Mr tests" ON)
option(MR_BUILD_BENCH "Build Mr benchmarks" ON)
option(MR_BUILD_EXAMPLES "Build Mr examples" ON)
option(MR_BUILD_CLI "Build Mr command-line tooling" ON)
option(MR_BUILD_DISTRIBUTED "Build coordinator/worker executables" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  add_compile_options(/W4 /WX /permissive- /Zc:__cplusplus)
  add_compile_options(/utf-8)
  add_compile_options(/EHsc)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

include(GNUInstallDirs)

add_subdirectory(src)
add_subdirectory(cmake)

if(MR_BUILD_CLI)
  add_subdirectory(tools)
endif()

if(MR_BUILD_DISTRIBUTED)
  add_subdirectory(distributed)
endif()

if(MR_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

if(MR_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(MR_BUILD_BENCH)
  add_subdirectory(bench)
endif()
