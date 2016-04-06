add_definitions(-DBOOST_LOG_DYN_LINK)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++11 -pthread -Wall -Wno-reorder")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake/Modules/")

if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -undefined dynamic_lookup")
  link_directories(/usr/local/lib /usr/local/opt/gettext/lib)
  include_directories(/usr/local/opt/gettext/include)
  set(CMAKE_MACOSX_RPATH 1)
  set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/opt/X11/lib/pkgconfig")
endif()

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

find_package(LibClang REQUIRED)
find_package(Boost 1.54 COMPONENTS thread log regex system filesystem REQUIRED)
find_package(ASPELL REQUIRED)
set(LIBCLANGMM_INCLUDE_DIR ../libclangmm/src)
set(TINY_PROCESS_INCLUDE_DIR ../tiny-process-library)

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

include(FindPkgConfig)
pkg_check_modules(GTKMM gtkmm-3.0 REQUIRED)
pkg_check_modules(GTKSVMM gtksourceviewmm-3.0 REQUIRED)

set(global_includes
   ${Boost_INCLUDE_DIRS}
   ${GTKMM_INCLUDE_DIRS}
   ${GTKSVMM_INCLUDE_DIRS}
   ${LIBCLANG_INCLUDE_DIRS}
   ${LIBCLANGMM_INCLUDE_DIR}
   ${ASPELL_INCLUDE_DIR}
   ${TINY_PROCESS_INCLUDE_DIR}
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
    juci.cc
    juci.h
    logging.h
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
add_executable(${project_name} ${project_files})
target_link_libraries(${project_name} ${global_libraries})

install(TARGETS ${project_name}
  RUNTIME DESTINATION bin
)
