cmake_minimum_required (VERSION 2.8.8)

project(juci)
set(JUCI_VERSION "1.2.1.2")

set(CPACK_PACKAGE_NAME "jucipp")
set(CPACK_PACKAGE_CONTACT "Ole Christian Eidheim <eidheim@gmail.com>")
set(CPACK_PACKAGE_VENDOR ${CPACK_PACKAGE_CONTACT})
set(CPACK_PACKAGE_VERSION ${JUCI_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A lightweight, platform independent C++-IDE with support for C++11, C++14, and experimental C++17 features depending on libclang version.")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_DEBIAN_PACKAGE_DEPENDS "cmake, make, g++, libclang-dev, liblldb-3.5-dev, clang-format-3.5, pkg-config, libboost-system-dev, libboost-filesystem-dev, libgtksourceviewmm-3.0-dev, aspell-en, libaspell-dev, libgit2-dev, exuberant-ctags")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/cppit/jucipp")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
include(CPack)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DJUCI_VERSION=\\\"${JUCI_VERSION}\\\" -std=c++1y -pthread -Wall -Wextra -Wno-unused-parameter -Wno-reorder")
if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -undefined dynamic_lookup") #TODO: fix this
  link_directories(/usr/local/lib /usr/local/opt/gettext/lib)
  include_directories(/usr/local/opt/gettext/include)
  set(CMAKE_MACOSX_RPATH 1)
  set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/opt/X11/lib/pkgconfig")
endif()
if(${CMAKE_SYSTEM_NAME} MATCHES FreeBSD)
  link_directories(/usr/local/lib)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules/")
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
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DJUCI_ENABLE_DEBUG")
else()
  set(LIBLLDB_LIBRARIES "")
  message("liblldb not found. Building juCi++ without debugging support")
endif()

find_package(Boost 1.54 COMPONENTS system filesystem REQUIRED)
find_package(ASPELL REQUIRED)
include(FindPkgConfig)
pkg_check_modules(GTKMM gtkmm-3.0 REQUIRED)
pkg_check_modules(GTKSVMM gtksourceviewmm-3.0 REQUIRED)
pkg_check_modules(LIBGIT2 libgit2 REQUIRED)

if(MSYS)
  set(global_libraries winpthread)
endif()
set(global_libraries ${global_libraries}
  ${GTKMM_LIBRARIES}
  ${GTKSVMM_LIBRARIES}
  ${Boost_LIBRARIES}
  ${LIBCLANG_LIBRARIES}
  ${LIBLLDB_LIBRARIES}
  ${ASPELL_LIBRARIES}
  ${LIBGIT2_LIBRARIES}
  )

set(global_includes
   ${Boost_INCLUDE_DIRS}
   ${GTKMM_INCLUDE_DIRS}
   ${GTKSVMM_INCLUDE_DIRS}
   ${LIBCLANG_INCLUDE_DIRS}
   ${ASPELL_INCLUDE_DIR}
   ${LIBGIT2_INCLUDE_DIRS}
   ${PROJECT_SOURCE_DIR}/libclangmm/src
   ${PROJECT_SOURCE_DIR}/tiny-process-library
   ${PROJECT_SOURCE_DIR}/src
)

add_subdirectory("src")

#TODO: instead of the if-expression below, disable tests on Travis CI for clang++ builds
if(NOT (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") AND (NOT $ENV{distribution} STREQUAL "")))
  if(ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
  endif()
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES Linux|.*BSD|DragonFly)
    install(FILES "${CMAKE_SOURCE_DIR}/share/juci.desktop"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
endif()
