set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-access-control -DJUCI_TESTS_PATH=\\\"${CMAKE_CURRENT_SOURCE_DIR}\\\"")

set(global_includes
   ${Boost_INCLUDE_DIRS}
   ${GTKMM_INCLUDE_DIRS}
   ${GTKSVMM_INCLUDE_DIRS}
   ${LIBCLANG_INCLUDE_DIRS}
   ${ASPELL_INCLUDE_DIR}
   ../libclangmm/src
   ../tiny-process-library
   ../src
)

set(global_libraries
  ${GTKMM_LIBRARIES}
  ${GTKSVMM_LIBRARIES}
  ${Boost_LIBRARIES}
  ${LIBCLANG_LIBRARIES}
  ${LIBLLDB_LIBRARIES}
  ${ASPELL_LIBRARIES}
)

set(stub_files
    stubs/config.cc
    stubs/dialogs.cc
    stubs/info.cc
    stubs/selectiondialog.cc
    stubs/terminal.cc
    stubs/tooltips.cc
)

add_library(stubs OBJECT ${stub_files})

include_directories(${global_includes})

add_executable(cmake_build_test cmake_build_test.cc
               $<TARGET_OBJECTS:project_shared> $<TARGET_OBJECTS:stubs>)
target_link_libraries(cmake_build_test ${global_libraries})
add_test(cmake_build_test cmake_build_test)

add_executable(source_test source_test.cc
               $<TARGET_OBJECTS:project_shared> $<TARGET_OBJECTS:stubs>)
target_link_libraries(source_test ${global_libraries})
add_test(source_test source_test)

add_executable(source_clang_test source_clang_test.cc
               $<TARGET_OBJECTS:project_shared> $<TARGET_OBJECTS:stubs>)
target_link_libraries(source_clang_test ${global_libraries})
add_test(source_clang_test source_clang_test)
