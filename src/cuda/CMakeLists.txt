# CUDA residency backend (real device memory, RTX 5090 / sm_120, CUDA 13.1).

add_library(mr_cuda STATIC cuda_backend.cu)
target_include_directories(mr_cuda
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(mr_cuda PUBLIC mr_core CUDA::cudart)
set_target_properties(mr_cuda PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CUDA_ARCHITECTURES 120
  CUDA_STANDARD 20
  CUDA_STANDARD_REQUIRED ON
)
if(MSVC)
  # Strict warnings for the CUDA host code, with only the known non-actionable
  # nvcc driver/header warnings suppressed. Our CUDA source itself stays clean.
  # /W4 for the CUDA host code; /WX is intentionally not used because nvcc's
  # generated stub code triggers non-actionable warnings (e.g. C4211). Our CUDA
  # source is kept warning-clean under /W4.
  target_compile_options(mr_cuda PRIVATE
    -Xcompiler=/W4
    -Xcompiler=/wd4211 -Xcompiler=/wd4514 -Xcompiler=/wd4710 -Xcompiler=/wd4820 -Xcompiler=/wd4458)
endif()

# Real cold-load -> reuse -> eviction -> reload device-memory proof.
if(MR_BUILD_TESTS)
  add_executable(cuda_residency_proof cuda_proof.cpp)
  target_link_libraries(cuda_residency_proof PRIVATE mr_core mr_cuda CUDA::cudart)
  add_test(NAME mr_cuda_residency_proof COMMAND cuda_residency_proof)
endif()