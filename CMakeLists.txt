cmake_minimum_required (VERSION 2.8.8)

project(juci)
set(JUCI_VERSION "1.4.4")

set(CPACK_PACKAGE_NAME "jucipp")
set(CPACK_PACKAGE_CONTACT "Ole Christian Eidheim <eidheim@gmail.com>")
set(CPACK_PACKAGE_VENDOR ${CPACK_PACKAGE_CONTACT})
set(CPACK_PACKAGE_VERSION ${JUCI_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A lightweight, platform independent C++-IDE with support for C++11, C++14, and experimental C++17 features depending on libclang version.")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_DEBIAN_PACKAGE_DEPENDS "cmake, make, g++, libclang-3.8-dev, liblldb-3.8-dev, clang-format, pkg-config, libboost-system-dev, libboost-filesystem-dev, libboost-serialization-dev libgtksourceviewmm-3.0-dev, aspell-en, libaspell-dev, libgit2-dev, exuberant-ctags")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/cppit/jucipp")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
include(CPack)

add_compile_options(-std=c++1y -pthread -Wall -Wextra -Wno-unused-parameter)
add_definitions(-DJUCI_VERSION="${JUCI_VERSION}")
if(CMAKE_BUILD_TYPE STREQUAL "")
  add_compile_options(-O3)
endif()

# temporarily disable these warnings: Only <gtksourceview/gtksource.h> can be included directly.
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  add_compile_options(-Wno-cpp)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options("-Wno-#warnings")
endif()

if(APPLE)
  link_directories(/usr/local/lib /usr/local/opt/gettext/lib)
  include_directories(/usr/local/opt/gettext/include)
  set(CMAKE_MACOSX_RPATH 1)
  set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/opt/X11/lib/pkgconfig")
endif()
if(${CMAKE_SYSTEM_NAME} MATCHES FreeBSD)
  link_directories(/usr/local/lib)
endif()

option(BUILD_TESTING OFF)

set(BUILD_TESTING_SAVED ${BUILD_TESTING})
set(BUILD_TESTING OFF CACHE BOOL "Disable sub-project tests" FORCE)
add_subdirectory(libclangmm)
add_subdirectory(tiny-process-library)
set(BUILD_TESTING ${BUILD_TESTING_SAVED} CACHE BOOL "Set to previous value" FORCE)

find_package(Boost 1.54 COMPONENTS system filesystem serialization REQUIRED)
find_package(ASPELL REQUIRED)
include(FindPkgConfig)
pkg_check_modules(GTKMM gtkmm-3.0 REQUIRED)
pkg_check_modules(GTKSVMM gtksourceviewmm-3.0 REQUIRED)
pkg_check_modules(LIBGIT2 libgit2 REQUIRED)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/libclangmm/cmake_modules/")
find_package(LibClang REQUIRED)
#Find liblldb with the same version as the version of libclang found
string(REPLACE libclang liblldb LIBLLDB_LIBRARIES "${LIBCLANG_LIBRARIES}")
if(EXISTS "${LIBLLDB_LIBRARIES}")
  set(LIBLLDB_FOUND TRUE)
elseif(EXISTS "${LIBLLDB_LIBRARIES}.1")
  set(LIBLLDB_LIBRARIES "${LIBLLDB_LIBRARIES}.1")
  set(LIBLLDB_FOUND TRUE)
endif()
if(LIBLLDB_FOUND)
  add_definitions(-DJUCI_ENABLE_DEBUG)
else()
  set(LIBLLDB_LIBRARIES "")
  message("liblldb not found. Building juCi++ without debugging support")
endif()

# For both src and tests targets
include_directories(
  ${Boost_INCLUDE_DIRS}
  ${GTKMM_INCLUDE_DIRS}
  ${GTKSVMM_INCLUDE_DIRS}
  ${LIBCLANG_INCLUDE_DIRS}
  ${ASPELL_INCLUDE_DIR}
  ${LIBGIT2_INCLUDE_DIRS}
)

add_subdirectory("src")

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
