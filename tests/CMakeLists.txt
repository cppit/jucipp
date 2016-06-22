set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-access-control -DJUCI_BUILD_PATH=\\\"${CMAKE_BINARY_DIR}\\\" -DJUCI_TESTS_PATH=\\\"${CMAKE_CURRENT_SOURCE_DIR}\\\"")

set(global_includes
   ${Boost_INCLUDE_DIRS}
   ${GTKMM_INCLUDE_DIRS}
   ${GTKSVMM_INCLUDE_DIRS}
   ${LIBCLANG_INCLUDE_DIRS}
   ${ASPELL_INCLUDE_DIR}
   ${LIBGIT2_INCLUDE_DIRS}
   ../libclangmm/src
   ../tiny-process-library
   ../src
)

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

set(stub_files
    stubs/config.cc
    stubs/dialogs.cc
    stubs/directories.cc
    stubs/info.cc
    stubs/selectiondialog.cc
    stubs/terminal.cc
    stubs/tooltips.cc
)

add_library(stubs OBJECT ${stub_files})

include_directories(${global_includes})

add_executable(process_test process_test.cc
               $<TARGET_OBJECTS:project_shared> $<TARGET_OBJECTS:stubs>)
target_link_libraries(process_test ${global_libraries})
add_test(process_test process_test)

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

if(LIBLLDB_FOUND AND (NOT LIBCLANG_LIBRARIES STREQUAL "/usr/lib/llvm-3.5/lib/libclang.so"))
  add_executable(lldb_test lldb_test.cc
                 $<TARGET_OBJECTS:project_shared> $<TARGET_OBJECTS:stubs>)
  target_link_libraries(lldb_test ${global_libraries})
  add_test(lldb_test lldb_test)
  add_subdirectory("lldb_test_files")
endif()

add_executable(git_test git_test.cc
               $<TARGET_OBJECTS:project_shared> $<TARGET_OBJECTS:stubs>)
target_link_libraries(git_test ${global_libraries})
add_test(git_test git_test)
