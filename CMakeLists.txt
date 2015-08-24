cmake_minimum_required (VERSION 2.8.4)

set(project_name juci)
#set(module juci_to_python_api)

if(MSYS)
  string(TOLOWER "/$ENV{MSYSTEM}" MSYS_PATH)
  set(bin_install_path "${MSYS_PATH}/bin")
else()
  set(bin_install_path "/usr/local/bin")
endif()

#set(lib_install_path "/usr/local/lib/python2.7/dist-packages/")

project (${project_name})

add_subdirectory("src")
