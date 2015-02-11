cmake_minimum_required (VERSION 2.4)
set(project_name plugin)
project (${project_name})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

include(FindPkgConfig)

#Boost_FOUND            - True if headers and requested libraries were found
#Boost_INCLUDE_DIRS     - Boost include directories
#Boost_LIBRARY_DIRS     - Link directories for Boost libraries
#Boost_LIBRARIES        - Boost component libraries to be linked
find_package(Boost 1.55 REQUIRED COMPONENTS python system  )

if(!${Boost_FOUND})
  message(FATAL_ERROR "Boost libraries not found")
endif()

#GTKMM_FOUND            - True if headers and requested libraries were found
#GTKMM_INCLUDE_DIRS     - GTKMM include directories
#GTKMM_LIBRARY_DIRS     - Link directories for GTKMM libraries
#GTKMM_LIBRARIES        - GTKMM component libraries to be linked
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)

if(!${GTKMM_FOUND})
  message(FATAL_ERROR "Gtkmm not found")
endif()

#PYTHONLIBS_FOUND           - True if headers and requested libraries were found
#PYTHON_INCLUDE_DIRS        - GTKMM include directories
#PYTHON_LIBRARIES           - GTKMM component libraries to be linked
#$PYTHONLIBS_VERSION_STRING - ersion string
find_package(PythonLibs 2.7 REQUIRED)

if(!${PYTHONLIBS_FOUND})
  message(FATAL_ERROR "Pythonlibs not found")
else()
  message("Found python libs ${PYTHONLIBS_VERSION_STRING}")
endif()

ADD_LIBRARY(plugintest_ext MODULE
  plugin.h
  plugin.cc
  plugin_ext.cc
  )

set_target_properties(plugintest_ext PROPERTIES PREFIX "") 
target_link_libraries(plugintest_ext ${Boost_LIBRARIES})

add_executable(${project_name}
  plugin.h
  plugin.cc
  )

include_directories(${PYTHON_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${GTKMM_INCLUDE_DIRS})
link_directories(${GTKMM_LIBRARY_DIRS} ${Boost_LIBRARY_DIRS}  ${PYTHON_LIBRARIES})
target_link_libraries(${project_name} ${Boost_LIBRARIES} ${PYTHON_LIBRARIES} ${GTKMM_LIBRARIES})