cmake_minimum_required(VERSION 2.8.7)
project(libdw_bcc)

include(cmake/GetGitRevisionDescription.cmake)
include(cmake/version.cmake)
include(CMakeDependentOption)
include(GNUInstallDirs)
include(CheckCXXCompilerFlag)
include(cmake/FindCompilerFlag.cmake)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(tests)
add_subdirectory(cases)
