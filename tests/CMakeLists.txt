add_definitions(-DBOOST_LOG_DYN_LINK)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread -Wall -Wextra -Wno-unused-parameter -Wno-reorder")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DJUCI_TESTS_PATH=\\\"${CMAKE_CURRENT_SOURCE_DIR}\\\" -Wl,--unresolved-symbols=ignore-in-object-files")
else()
  message(WARNING "testing only supported for g++")
  return()
endif()

if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -undefined dynamic_lookup")
  link_directories(/usr/local/lib /usr/local/opt/gettext/lib)
  include_directories(/usr/local/opt/gettext/include)
  set(CMAKE_MACOSX_RPATH 1)
  set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/opt/X11/lib/pkgconfig")
endif()

find_package(Boost 1.54 COMPONENTS regex system filesystem REQUIRED)
set(LIBCLANGMM_INCLUDE_DIR ../libclangmm/src)

include(FindPkgConfig)
pkg_check_modules(GTKMM gtkmm-3.0 REQUIRED)

set(global_includes
   ${Boost_INCLUDE_DIRS}
   ${GTKMM_INCLUDE_DIRS}
   ${GTKSVMM_INCLUDE_DIRS}
   ../src
   ../tiny-process-library
)

set(global_libraries
  ${GTKMM_LIBRARIES}
  ${GTKSVMM_LIBRARIES}
  ${Boost_LIBRARIES}
)

include_directories(${global_includes})

add_executable(cmake_test cmake_test.cc
               ../src/filesystem.cc ../src/cmake.cc ../src/project_build.cc)
target_link_libraries(cmake_test ${global_libraries})
add_test(cmake_test cmake_test)

#Added for example only, and requires display server to work
#However, it is possible to use the Broadway backend if the test is run in a pure terminal environment:
#broadwayd&
#make test
add_executable(example_test example_test.cc
               stubs/dispatcher.cc stubs/terminal.cc stubs/info.cc)
target_link_libraries(example_test ${global_libraries})
add_test(example_test example_test)
