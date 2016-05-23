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

set(global_includes
   ${Boost_INCLUDE_DIRS}
   ${GTKMM_INCLUDE_DIRS}
   ${GTKSVMM_INCLUDE_DIRS}
   ${LIBCLANG_INCLUDE_DIRS}
   ${ASPELL_INCLUDE_DIR}
   ../libclangmm/src
   ../tiny-process-library
)

set(global_libraries
  ${LIBCLANG_LIBRARIES}
  ${GTKMM_LIBRARIES}
  ${GTKSVMM_LIBRARIES}
  ${Boost_LIBRARIES}
  ${ASPELL_LIBRARIES}
  ${LIBLLDB_LIBRARIES}
)

set(project_files
    cmake.cc
    cmake.h
    config.cc
    config.h
    dialogs.cc
    dialogs.h
    directories.cc
    directories.h
    dispatcher.cc
    dispatcher.h
    entrybox.cc
    entrybox.h
    files.h
    filesystem.cc
    filesystem.h
    info.h
    info.cc
    juci.cc
    juci.h
    menu.cc
    menu.h
    notebook.cc
    notebook.h
    project.cc
    project.h
    project_build.h
    project_build.cc
    selectiondialog.cc
    selectiondialog.h
    source.cc
    source.h
    source_clang.cc
    source_clang.h
    terminal.cc
    terminal.h
    tooltips.cc
    tooltips.h
    window.cc
    window.h

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

    ../tiny-process-library/process.cpp)

if(LIBLLDB_FOUND)
  list(APPEND project_files debug_clang.h debug_clang.cc)
endif()

if(MSYS)
  list(APPEND project_files dialogs_unix.cc ../tiny-process-library/process_win.cpp)
else()
  list(APPEND project_files dialogs_unix.cc ../tiny-process-library/process_unix.cpp)
endif()

include_directories(${global_includes})

if(("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") AND (NOT $ENV{distribution} STREQUAL ""))
  add_library(${project_name} OBJECT ${project_files})
else()
  add_executable(${project_name} ${project_files})
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
