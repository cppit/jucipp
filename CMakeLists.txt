cmake_minimum_required (VERSION 2.8.4)

set(project_name juci)
#set(module juci_to_python_api)

#set(lib_install_path "/usr/local/lib/python2.7/dist-packages/")

project (${project_name})

add_subdirectory("src")

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD|OpenBSD|NetBSD")
    install(FILES "${CMAKE_SOURCE_DIR}/share/juci.desktop"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
endif()
