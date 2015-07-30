add_definitions(-DBOOST_LOG_DYN_LINK)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++11 -pthread -Wall -Wno-reorder")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake/Modules/")

if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L/usr/local/opt/gettext/lib -I/usr/local/opt/gettext/include -undefined dynamic_lookup") #T
  set(CMAKE_MACOSX_RPATH 1)
  set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/opt/X11/lib/pkgconfig")
endif()
INCLUDE(FindPkgConfig)

set(validation true)

function(install_help APPLE UNIX WINDOWS)
  message("Install package with:")
  if(UNIX)
    if(APPLE)
      message("brew install ${APPLE}")
    else()
      message("sudo apt-get install ${UNIX}")
    endif(APPLE)
  endif(UNIX)
  if(WINDOWS)
    message("choco install ${WINDOWS}")
  endif(WINDOWS)
endfunction(install_help)

function(validate FOUND APPLE UNIX WINDOWS)
  if(!${FOUND})
    set(validation false)
    install_help(${APPLE} ${UNIX} ${WINDOWS})
  endif()
endfunction(validate)

find_package(LibClangmm)
validate(${LCL_FOUND} "clangmm" "clangmm" "clangmm")

find_package(LibClang)
validate(${LIBCLANG_FOUND} "clang" "libclang-dev" "llvm")

find_package(PythonLibs 2.7)
validate(${PYTHONLIBS_FOUND} "python" "libpython-dev" "python") 

find_package(Boost 1.55 COMPONENTS python thread log system filesystem REQUIRED)
validate(${Boost_FOUND} "boost" "libboost-all-dev" "boost")

pkg_check_modules(GTKMM gtkmm-3.0) # The name GTKMM is set here for the variables abouve
validate(${GTKMM_FOUND} "gtkmm" "libgtkmm-dev" "gtkmm")

pkg_check_modules(GTKSVMM gtksourceviewmm-3.0)
validate(${GTKSVMM_FOUND} "gtksvmm" "libgtksvmm-dev" "gtkmmsv")

if(${validation})
  add_executable(${project_name}
    juci.h
    juci.cc	
    menu.h
    menu.cc
    source.h
    source.cc
    selectiondialog.h
    selectiondialog.cc
    config.h
    config.cc
    sourcefile.h
    sourcefile.cc
    window.cc
    window.h
    api.h
    api.cc
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
    singletons.cc)

  add_library(${module} SHARED
    api
    api_ext)

  include_directories(
    ${Boost_INCLUDE_DIRS}
    ${PYTHON_INCLUDE_DIRS}
    ${GTKMM_INCLUDE_DIRS}
    ${GTKSVMM_INCLUDE_DIRS}
    ${LCL_INCLUDE_DIRS}
    ${LIBCLANG_INCLUDE_DIRS})

  link_directories(
    ${GTKMM_LIBRARY_DIRS}
    ${GTKSVMM_LIBRARY_DIRS}
    ${Boost_LIBRARY_DIRS}
    ${PYTHON_INCLUDE_DIRS}
    ${LCL_LIBRARY_DIRS}
    ${LIBCLANG_LIBRARY_DIRS})

  set_target_properties(${module}
    PROPERTIES PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/")

  target_link_libraries(${module} ${PYTHON_LIBRARIES} ${Boost_LIBRARIES})

  target_link_libraries(${project_name}
    ${LIBCLANG_LIBRARIES}
    ${LCL_LIBRARIES}
    ${GTKMM_LIBRARIES}
    ${GTKSVMM_LIBRARIES}
    ${Boost_LIBRARIES}
    ${PYTHON_LIBRARIES})

  install(TARGETS ${project_name} ${module}
    RUNTIME DESTINATION ${bin_install_path}
    LIBRARY DESTINATION ${lib_install_path}
    )
endif(${validation})

