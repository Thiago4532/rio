cmake_minimum_required(VERSION 3.13.0)
# Determine if rio is built as a subproject (using add_subdirectory)
# or if it is the master project.
if (NOT DEFINED RIO_MASTER_PROJECT)
  set(RIO_MASTER_PROJECT OFF)
  if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(RIO_MASTER_PROJECT ON)
  endif ()
endif ()

project(rio)

option(RIO_TEST "Generate the test target." ${RIO_MASTER_PROJECT})

include(GNUInstallDirs)

add_subdirectory(tsl)

if (RIO_MASTER_PROJECT)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
endif()

if (NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif ()

set(RIO_SOURCES
  src/rio/event_loop.cpp
  src/rio/selector.cpp
  src/rio/time_type.cpp
)

add_library(rio ${RIO_SOURCES})

target_sources(rio
  PUBLIC
  FILE_SET HEADERS
    TYPE HEADERS
    BASE_DIRS
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    FILES
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/rio>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/rio>
)

target_compile_features(rio PUBLIC cxx_std_20)
target_link_libraries(rio PUBLIC tsl)

if (RIO_TEST)
  add_subdirectory(tests)
endif ()

install(TARGETS rio
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# vim: et ts=8 sts=2 sw=2

