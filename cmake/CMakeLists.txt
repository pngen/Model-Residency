# Installation and exported CMake package configuration for Model Residency.

install(TARGETS mr_core
  EXPORT ModelResidencyTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/mr
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp"
)

# Compute the relative path from the install prefix to each exported target.
include(CMakePackageConfigHelpers)

set(MR_INSTALL_CMAKEDIR ${CMAKE_INSTALL_LIBDIR}/cmake/ModelResidency)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/ModelResidencyConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ModelResidencyConfig.cmake
  INSTALL_DESTINATION ${MR_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/ModelResidencyConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(EXPORT ModelResidencyTargets
  FILE ModelResidencyTargets.cmake
  NAMESPACE ModelResidency::
  DESTINATION ${MR_INSTALL_CMAKEDIR}
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/ModelResidencyConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/ModelResidencyConfigVersion.cmake
  DESTINATION ${MR_INSTALL_CMAKEDIR}
)
