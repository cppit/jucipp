cmake_minimum_required (VERSION 2.8.4)
set(project_name juci)
set(module juci_to_python_api)

project (${project_name})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake/Modules/")

INCLUDE(FindPkgConfig)

message("Searcing for libclang")
#LIBCLANG_FOUND System has libclang.
#LIBCLANG_INCLUDE_DIRS The libclang include directories.
#LIBCLANG_LIBRARIES  The libraries needed to use libclang.
#LIBCLANG_LIBRARY_DIR The path to the directory containing libclang.
#LIBCLANG_KNOWN_LLVM_VERSIONS  Known LLVM release numbers.
find_package(LibClangmm)
find_package(LibClang)

if(${LCL_FOUND}) 
  message("libclangmm libraries found. Continuing")
  message("${LCL_INCLUDE_DIRS}")
else()
  message(FATAL_ERROR "The libclangmm libraries are required. Quitting.")
endif()

if(${LIBCLANG_FOUND}) 
  message("libclangmm libraries found. Continuing")
  message("${LIBCLANG_INCLUDE_DIRS}")
else()
  message(FATAL_ERROR "The libclangmm libraries are required. Quitting.")
endif()

#### Finding boost, the variables below is set #####
#PYTHONLIBS_FOUND            - True if headers and requested libraries were found
#PYTHON_INCLUDE_DIRS     - Boost include directories
#PYTHON_LIBRARIES        - Boost component libraries to be linked
find_package(PythonLibs 2.7)

#If python is found
if(${PYTHONLIBS_FOUND}) 
  message("Python libraries found. Continuing")
else()
  message("Please install python libraries. The libraries where not found.")
  message("Python include dirs: ${PYTHON_INCLUDE_DIRS}")
  message("Python link dirs ${PYTHON_LIBRARIES}")
  message(FATAL_ERROR "The python libraries are required. Quitting.")
endif()

#### Finding boost, the variables below is set #####
#Boost_FOUND            - True if headers and requested libraries were found
#Boost_INCLUDE_DIRS     - Boost include directories
#Boost_LIBRARY_DIRS     - Link directories for Boost libraries
#Boost_LIBRARIES        - Boost component libraries to be linked
find_package(Boost 1.5 REQUIRED COMPONENTS python timer system)

#If boost is not found
if(${Boost_FOUND}) 
  message("Boost libraries found. Continuing")
else()
  message("Please install boost libraries. The libraries where not found.")
  message("Boost library dirs: ${Boost_LIBRARY_DIRS}")
  message("Boost include dirs: ${Boost_INCLUDE_DIRS}")
  message("Boost link dirs ${Boost_LIBRARIES}")
  message(FATAL_ERROR "The boost libraries are required. Quitting.")
endif()

#### Finding gtkmm, the variables below is set #####
#GTKMM_FOUND            - True if headers and requested libraries were found
#GTKMM_INCLUDE_DIRS     - GTKMM include directories
#GTKMM_LIBRARY_DIRS     - Link directories for GTKMM libraries
#GTKMM_LIBRARIES        - GTKMM libraries to be linked
pkg_check_modules(GTKMM gtkmm-3.0) # The name GTKMM is set here for the variables abouve

#If gtkmm is not found
if(${GTKMM_FOUND})
  message("Gtkmm libraries found. Continuing")
else()
  message("Please install gtkmm libraries. The libraries where not found.")
  message("Gtkmm library dirs ${GTKMM_LIBRARY_DIRS}")
  message("Gtkmm include dirs ${GTKMM_INCLUDE_DIRS}")
  message("Gtkmm link dirs ${GTKMM_LIBRARIES}")
  message(FATAL_ERROR "The gtkmm libraries are required. Quitting.")
endif()
# name of the executable on Windows will be example.exe
add_executable(${project_name}
  #list of every needed file to create the executable
  juci.cc	
  keybindings.h
  keybindings.cc
  menu.h
  menu.cc
  source.h
  source.cc
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
  entry.h
  entry.cc
  )

add_library(${module} SHARED
  api
  api_ext
  )
# dependencies

include_directories(
  ${Boost_INCLUDE_DIRS}
  ${PYTHON_INCLUDE_DIRS}
  ${GTKMM_INCLUDE_DIRS}
  ${LCL_INCLUDE_DIRS}
  ${LIBCLANG_INCLUDE_DIRS}
  )
link_directories(
  ${GTKMM_LIBRARY_DIRS}
  ${Boost_LIBRARY_DIRS}
  ${PYTHON_INCLUDE_DIRS}
  ${LCL_LIBRARY_DIRS}
  ${LIBCLANG_LIBRARY_DIRS}
  )
#module:
#set_target_properties(${module} PROPERTIES PREFIX ""
# LIBRARY_OUTPUT_DIRECTORY "/usr/lib/python2.7/dist-packages/")
target_link_libraries(${module} ${PYTHON_LIBRARIES} ${Boost_LIBRARIES})
#executable:
target_link_libraries(${project_name} ${LIVCLANG_LIBRARIES} ${LCL_LIBRARIES} ${GTKMM_LIBRARIES} ${Boost_LIBRARIES} ${PYTHON_LIBRARIES})


