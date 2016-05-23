cmake_minimum_required (VERSION 2.8.4)

set(project_name juci)
project (${project_name})

add_subdirectory("src")

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD|OpenBSD|NetBSD")
    install(FILES "${CMAKE_SOURCE_DIR}/share/juci.desktop"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
endif()

if(NOT (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") AND (NOT $ENV{distribution} STREQUAL "")))
  if(ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
  endif()
endif()
