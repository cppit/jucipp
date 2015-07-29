cmake_minimum_required (VERSION 2.8.4)

set(project_name juci)
set(module juci_to_python_api)

#### TODO WINDOWS SUPPORT ####
set(juci_config_folder "$ENV{HOME}/.juci")
set(bin_install_path "/usr/local/bin")
set(lib_install_path "/usr/local/lib/python2.7/dist-packages/")
#####

project (${project_name})

add_subdirectory("src")

install(
  DIRECTORY "config" "plugins"
  DESTINATION ${juci_config_folder}
  USE_SOURCE_PERMISSIONS)
install(CODE "FILE(MAKE_DIRECTORY ${juci_config_filder}/log)")
