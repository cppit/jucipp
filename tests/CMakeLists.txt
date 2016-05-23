add_definitions(-DBOOST_LOG_DYN_LINK)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-access-control -std=c++11 -pthread -Wall -Wextra -Wno-unused-parameter -Wno-reorder")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DJUCI_TESTS_PATH=\\\"${CMAKE_CURRENT_SOURCE_DIR}\\\"")

if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -undefined dynamic_lookup")
  link_directories(/usr/local/lib /usr/local/opt/gettext/lib)
  include_directories(/usr/local/opt/gettext/include)
  set(CMAKE_MACOSX_RPATH 1)
  set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/opt/X11/lib/pkgconfig")
endif()

find_package(Boost 1.54 COMPONENTS regex system filesystem REQUIRED)
find_package(ASPELL REQUIRED)
set(LIBCLANGMM_INCLUDE_DIR ../libclangmm/src)

include(FindPkgConfig)
pkg_check_modules(GTKMM gtkmm-3.0 REQUIRED)

set(global_includes
   ${Boost_INCLUDE_DIRS}
   ${GTKMM_INCLUDE_DIRS}
   ${GTKSVMM_INCLUDE_DIRS}
   ${ASPELL_INCLUDE_DIR}
   ../src
   ../tiny-process-library
)

set(global_libraries
  ${GTKMM_LIBRARIES}
  ${GTKSVMM_LIBRARIES}
  ${Boost_LIBRARIES}
  ${ASPELL_LIBRARIES}
)

set(stub_sources
    stubs/config.cc
    stubs/dialogs.cc
    stubs/dispatcher.cc
    stubs/info.cc
    stubs/selectiondialog.cc
    stubs/terminal.cc
    stubs/tooltips.cc
)

include_directories(${global_includes})

add_library(stubs_library ${stub_sources})

add_executable(cmake_build_test cmake_build_test.cc
               ../src/filesystem.cc ../src/cmake.cc ../src/project_build.cc)
target_link_libraries(cmake_build_test ${global_libraries} stubs_library)
add_test(cmake_build_test cmake_build_test)

add_executable(source_test source_test.cc
               ../src/source.cc)
target_link_libraries(source_test ${global_libraries} stubs_library)
add_test(source_test source_test)
