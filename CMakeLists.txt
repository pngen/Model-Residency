cmake_minimum_required(VERSION 3.24)

project(ModelResidency VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MR_ENABLE_CUDA "Enable CUDA residency validation backend" ON)

if(MR_ENABLE_CUDA)
  find_package(CUDAToolkit QUIET)
  if(CUDAToolkit_FOUND)
    enable_language(CUDA)
  endif()
endif()
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

# Strict warnings apply to C++ code only, via CMAKE_CXX_FLAGS. The CUDA
# compiler uses CMAKE_CUDA_FLAGS, so bare /W4 /WX are never handed to nvcc
# (which would misinterpret them); the CUDA target sets its own -Xcompiler flags.
if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /WX /permissive- /Zc:__cplusplus /utf-8 /EHsc")
  # CUDA host flags are set per-target via -Xcompiler; avoid bare MSVC flags
  # (nvcc misinterprets them as inputs) and avoid /WX (nvcc-generated stub code
  # emits non-actionable warnings). The host library keeps strict /W4 /WX.
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")
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