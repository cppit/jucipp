cmake_minimum_required (VERSION 2.8.4)
project (juci)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

INCLUDE(FindPkgConfig)

# name of the executable on Windows will be example.exe
add_executable(juci
# list of every needed file to create the executable
    model.h
    models.cc

    view.h
    views.cc

    controller.h
    controllers.cc

    window.cc
    juci.cc
    )

# dependencies
    pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
    include_directories(${GTKMM_INCLUDE_DIRS} )
    link_directories(${GTKMM_LIBRARY_DIRS})
    target_link_libraries(juci ${GTKMM_LIBRARIES})