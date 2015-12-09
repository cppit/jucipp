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

if(UNIX) #Checking if compiling on Ubuntu that has a buggy menu system
  find_program(LSB_RELEASE_BIN lsb_release)
  if(LSB_RELEASE_BIN)
    execute_process(COMMAND ${LSB_RELEASE_BIN} -is
      OUTPUT_VARIABLE DISTRIBUTION OUTPUT_STRIP_TRAILING_WHITESPACE)
    if((DISTRIBUTION STREQUAL Ubuntu) OR (DISTRIBUTION STREQUAL LinuxMint))
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DJUCI_UBUNTU_BUGGED_MENU")
    endif()
  endif()
endif()

if(MSYS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMSYS_PROCESS_USE_SH -DJUCI_CMAKE_INSTALL_PREFIX=\\\"${CMAKE_INSTALL_PREFIX}\\\"")
endif()

INCLUDE(FindPkgConfig)

find_package(LibClang REQUIRED)

#find_package(PythonLibs 2.7)

#find_package(Boost 1.55 COMPONENTS python thread log system filesystem REQUIRED)
find_package(Boost 1.54 COMPONENTS thread log system filesystem regex REQUIRED)

pkg_check_modules(GTKMM gtkmm-3.0 REQUIRED) # The name GTKMM is set here for the variables abouve

pkg_check_modules(GTKSVMM gtksourceviewmm-3.0 REQUIRED)

find_package(ASPELL REQUIRED)

set(source_files juci.h
    juci.cc	
    menu.h
    menu.cc
    source.h
    source.cc
    source_clang.h
    source_clang.cc
    selectiondialog.h
    selectiondialog.cc
    config.h
    config.cc
    filesystem.h
    filesystem.cc
    window.cc
    window.h
    dialogs.h
#    api.h
#    api.cc
    notebook.cc
    notebook.h
    entrybox.h
    entrybox.cc
    directories.h
    directories.cc
    terminal.h
    terminal.cc
    tooltips.h
    tooltips.cc
    singletons.h
    singletons.cc
    cmake.h
    cmake.cc
    dialogs.cc
    
    ../libclangmm/src/CodeCompleteResults.cc
    ../libclangmm/src/CompilationDatabase.cc
    ../libclangmm/src/CompileCommand.cc     
    ../libclangmm/src/CompileCommands.cc    
    ../libclangmm/src/CompletionString.cc   
    ../libclangmm/src/Cursor.cc
    ../libclangmm/src/Index.cc
    ../libclangmm/src/SourceLocation.cc
    ../libclangmm/src/SourceRange.cc
    ../libclangmm/src/Token.cc
    ../libclangmm/src/Tokens.cc
    ../libclangmm/src/TranslationUnit.cc
    ../libclangmm/src/Diagnostic.cc
    ../libclangmm/src/Utility.cc
    
    ../tiny-process-library/process.cpp)

if(MSYS)
  list(APPEND source_files dialogs_unix.cc) #dialogs_win.cc does not work any more because of missing SHCreateItemFromParsingName
  list(APPEND source_files ../tiny-process-library/process_win.cpp)
else()
  list(APPEND source_files dialogs_unix.cc)
  list(APPEND source_files ../tiny-process-library/process_unix.cpp)
endif()

add_executable(${project_name} ${source_files})

#  add_library(${module} SHARED
#    api
#    api_ext)

include_directories(
  ${Boost_INCLUDE_DIRS}
#    ${PYTHON_INCLUDE_DIRS}
  ${GTKMM_INCLUDE_DIRS}
  ${GTKSVMM_INCLUDE_DIRS}
  ${LIBCLANG_INCLUDE_DIRS}
  ${ASPELL_INCLUDE_DIR}
  ../libclangmm/src
  ../tiny-process-library
)

link_directories(
  ${GTKMM_LIBRARY_DIRS}
  ${GTKSVMM_LIBRARY_DIRS}
  ${Boost_LIBRARY_DIRS}
#    ${PYTHON_INCLUDE_DIRS}
  ${LIBCLANG_LIBRARY_DIRS}
)

#  set_target_properties(${module}
#    PROPERTIES PREFIX ""
#    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/")

#  target_link_libraries(${module} ${PYTHON_LIBRARIES} ${Boost_LIBRARIES})
#  target_link_libraries(${module} ${Boost_LIBRARIES})

target_link_libraries(${project_name}
  ${LIBCLANG_LIBRARIES}
  ${GTKMM_LIBRARIES}
  ${GTKSVMM_LIBRARIES}
  ${Boost_LIBRARIES}
  ${ASPELL_LIBRARIES}
  #${PYTHON_LIBRARIES}
)

#  install(TARGETS ${project_name} ${module}
install(TARGETS ${project_name}
  RUNTIME DESTINATION bin
#    LIBRARY DESTINATION ${lib_install_path}
)
