cmake_minimum_required (VERSION 2.8.4)

set(project_name juci)
project (${project_name})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread -Wall -Wextra -Wno-unused-parameter -Wno-reorder")
if(NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -undefined dynamic_lookup") #TODO: fix this
  link_directories(/usr/local/lib /usr/local/opt/gettext/lib)
  include_directories(/usr/local/opt/gettext/include)
  set(CMAKE_MACOSX_RPATH 1)
  set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/opt/X11/lib/pkgconfig")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules/")
find_package(LibClang REQUIRED)
find_package(Boost 1.54 COMPONENTS regex system filesystem REQUIRED)
find_package(ASPELL REQUIRED)
include(FindPkgConfig)
pkg_check_modules(GTKMM gtkmm-3.0 REQUIRED)
pkg_check_modules(GTKSVMM gtksourceviewmm-3.0 REQUIRED)

add_subdirectory("src")

#TODO: instead of the if-expression below, disable tests on Travis CI for clang++ builds
if(NOT (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") AND (NOT $ENV{distribution} STREQUAL "")))
  if(ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
  endif()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD|OpenBSD|NetBSD")
    install(FILES "${CMAKE_SOURCE_DIR}/share/juci.desktop"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
endif()
