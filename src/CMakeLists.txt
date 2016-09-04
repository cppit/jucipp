if(MSYS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMSYS_PROCESS_USE_SH")
endif()

set(project_files
    config.cc
    dialogs.cc
    dialogs_unix.cc
    directories.cc
    entrybox.cc
    info.cc
    juci.cc
    menu.cc
    notebook.cc
    project.cc
    selectiondialog.cc
    terminal.cc
    tooltips.cc
    window.cc
)

#Files used both in ../src and ../tests
set(project_shared_files
    cmake.cc
    ctags.cc
    dispatcher.cc
    filesystem.cc
    git.cc
    project_build.cc
    source.cc
    source_clang.cc
    source_diff.cc
    source_spellcheck.cc

    ${PROJECT_SOURCE_DIR}/libclangmm/src/CodeCompleteResults.cc
    ${PROJECT_SOURCE_DIR}/libclangmm/src/CompilationDatabase.cc
    ${PROJECT_SOURCE_DIR}/libclangmm/src/CompileCommand.cc
    ${PROJECT_SOURCE_DIR}/libclangmm/src/CompileCommands.cc
    ${PROJECT_SOURCE_DIR}/libclangmm/src/CompletionString.cc
    ${PROJECT_SOURCE_DIR}/libclangmm/src/Cursor.cc
    ${PROJECT_SOURCE_DIR}/libclangmm/src/Diagnostic.cc
    ${PROJECT_SOURCE_DIR}/libclangmm/src/Index.cc
    ${PROJECT_SOURCE_DIR}/libclangmm/src/SourceLocation.cc
    ${PROJECT_SOURCE_DIR}/libclangmm/src/SourceRange.cc
    ${PROJECT_SOURCE_DIR}/libclangmm/src/Token.cc
    ${PROJECT_SOURCE_DIR}/libclangmm/src/Tokens.cc
    ${PROJECT_SOURCE_DIR}/libclangmm/src/TranslationUnit.cc
    ${PROJECT_SOURCE_DIR}/libclangmm/src/Utility.cc
    
    ${PROJECT_SOURCE_DIR}/tiny-process-library/process.cpp
)

if(LIBLLDB_FOUND)
  list(APPEND project_shared_files debug_lldb.cc)
endif()

if(MSYS)
  list(APPEND project_shared_files ${PROJECT_SOURCE_DIR}/tiny-process-library/process_win.cpp)
else()
  list(APPEND project_shared_files ${PROJECT_SOURCE_DIR}/tiny-process-library/process_unix.cpp)
endif()

include_directories(${global_includes})

add_library(project_shared OBJECT ${project_shared_files})

if(("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") AND (NOT $ENV{distribution} STREQUAL ""))
  add_library(${CMAKE_PROJECT_NAME} OBJECT ${project_files})
else()
  add_executable(${CMAKE_PROJECT_NAME} ${project_files} $<TARGET_OBJECTS:project_shared>)
  target_link_libraries(${CMAKE_PROJECT_NAME} ${global_libraries})
  install(TARGETS ${CMAKE_PROJECT_NAME}
    RUNTIME DESTINATION bin
  )
endif()

# add a target to generate API documentation with Doxygen
find_package(Plantuml)
find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen to ${CMAKE_CURRENT_BINARY_DIR}" VERBATIM
  )
endif(DOXYGEN_FOUND)
