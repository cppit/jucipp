if(UNIX) #Checking if compiling on Ubuntu that for instance has a buggy menu system
  find_program(LSB_RELEASE_BIN lsb_release)
  if(LSB_RELEASE_BIN)
    execute_process(COMMAND ${LSB_RELEASE_BIN} -is
      OUTPUT_VARIABLE DISTRIBUTION OUTPUT_STRIP_TRAILING_WHITESPACE)
    if((DISTRIBUTION STREQUAL Ubuntu) OR (DISTRIBUTION STREQUAL LinuxMint))
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DJUCI_UBUNTU")
    endif()
  endif()
endif()

if(MSYS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMSYS_PROCESS_USE_SH")
endif()

set(global_includes
   ${Boost_INCLUDE_DIRS}
   ${GTKMM_INCLUDE_DIRS}
   ${GTKSVMM_INCLUDE_DIRS}
   ${LIBCLANG_INCLUDE_DIRS}
   ${ASPELL_INCLUDE_DIR}
   ../libclangmm/src
   ../tiny-process-library
   .
)

set(global_libraries
  ${GTKMM_LIBRARIES}
  ${GTKSVMM_LIBRARIES}
  ${Boost_LIBRARIES}
  ${LIBCLANG_LIBRARIES}
  ${LIBLLDB_LIBRARIES}
  ${ASPELL_LIBRARIES}
)

set(project_files
    config.cc
    dialogs.cc
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

    ../tiny-process-library/process.cpp
)

#Files used both in ../src and ../tests
set(project_shared_files
    cmake.cc
    dispatcher.cc
    filesystem.cc
    project_build.cc
    source.cc
    source_clang.cc

    ../libclangmm/src/CodeCompleteResults.cc
    ../libclangmm/src/CompilationDatabase.cc
    ../libclangmm/src/CompileCommand.cc     
    ../libclangmm/src/CompileCommands.cc    
    ../libclangmm/src/CompletionString.cc   
    ../libclangmm/src/Cursor.cc
    ../libclangmm/src/Diagnostic.cc
    ../libclangmm/src/Index.cc
    ../libclangmm/src/SourceLocation.cc
    ../libclangmm/src/SourceRange.cc
    ../libclangmm/src/Token.cc
    ../libclangmm/src/Tokens.cc
    ../libclangmm/src/TranslationUnit.cc
    ../libclangmm/src/Utility.cc
)

if(LIBLLDB_FOUND)
  list(APPEND project_shared_files debug_clang.cc)
endif()

if(MSYS)
  list(APPEND project_files dialogs_unix.cc ../tiny-process-library/process_win.cpp)
else()
  list(APPEND project_files dialogs_unix.cc ../tiny-process-library/process_unix.cpp)
endif()

include_directories(${global_includes})

add_library(project_shared OBJECT ${project_shared_files})

if(("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") AND (NOT $ENV{distribution} STREQUAL ""))
  add_library(${project_name} OBJECT ${project_files})
else()
  add_executable(${project_name} ${project_files} $<TARGET_OBJECTS:project_shared>)
  target_link_libraries(${project_name} ${global_libraries})
  install(TARGETS ${project_name}
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
