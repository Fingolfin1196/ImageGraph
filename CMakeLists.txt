cmake_minimum_required(VERSION 3.16)

project(ImageGraph)

option(BUILD_EXAMPLE "Build Examples" ON)
option(BUILD_TEST "Build Tests" ON)

add_subdirectory(libs/pcg-cpp)
include_directories(${PROJECT_SOURCE_DIR}/libs/pcg-cpp/include)

add_library(ImageGraph SHARED)

# C++ Standard Version and Compiler settings.
set_target_properties(ImageGraph PROPERTIES CXX_STANDARD 20)
target_compile_options(ImageGraph PRIVATE -Wpedantic -Werror -Wextra)
target_compile_options(ImageGraph PRIVATE $<$<CONFIG:RELEASE>:-Ofast;-march=native;-Wno-unused-parameter>)
target_compile_options(ImageGraph PRIVATE $<$<CONFIG:DEBUG>:-Og;-g>)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(ImageGraph PRIVATE -Wno-error=pass-failed)
endif()

# Boost
find_package(
  Boost
  COMPONENTS context
  REQUIRED)
target_link_libraries(ImageGraph PUBLIC Boost::context)

# PkgConfig
find_package(PkgConfig REQUIRED)
# VIPS
pkg_check_modules(VIPS REQUIRED IMPORTED_TARGET vips vips-cpp gobject-2.0)
target_link_libraries(ImageGraph PUBLIC PkgConfig::VIPS)

# Compatibility for pthread
find_package(Threads REQUIRED)
target_link_libraries(ImageGraph PUBLIC Threads::Threads)

# Threading Building Blocks
target_link_options(ImageGraph PUBLIC "-ltbb")

# PCG is now used through a relative path.

# Abseil
find_package(absl REQUIRED)
target_link_libraries(ImageGraph PUBLIC absl::flat_hash_map absl::flat_hash_set)

# Add files to compile
target_include_directories(ImageGraph PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                             $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_sources(ImageGraph PRIVATE src/core/NodeGraph.cpp src/internal/Task.cpp src/internal/GraphAdaptor.cpp
                                  src/internal/ProtoGraphAdaptor.cpp src/core/MemoryDistribution.cpp)

if(BUILD_TEST)
  add_subdirectory(test)
endif()
if(BUILD_EXAMPLE)
  add_subdirectory(example)
endif()

include(GNUInstallDirs)
install(
  TARGETS ImageGraph
  EXPORT ImageGraphTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImageGraph)
install(
  EXPORT ImageGraphTargets
  FILE ImageGraphTargets.cmake
  NAMESPACE ImageGraph::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ImageGraph)

include(CMakePackageConfigHelpers)
configure_package_config_file(
  ${CMAKE_CURRENT_LIST_DIR}/cmake/ImageGraphConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/ImageGraphConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ImageGraph)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ImageGraphConfig.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ImageGraph)
