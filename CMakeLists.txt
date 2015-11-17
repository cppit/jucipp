cmake_minimum_required (VERSION 2.8.4)

set(project_name juci)
#set(module juci_to_python_api)

#set(lib_install_path "/usr/local/lib/python2.7/dist-packages/")

project (${project_name})

add_subdirectory("src")

find_program(XDG_DESKTOP_MENU_EXECUTABLE xdg-desktop-menu)
if(XDG_DESKTOP_MENU_EXECUTABLE)
  file(MAKE_DIRECTORY "/usr/share/desktop-directories") #Workaround for https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=730621
  install(CODE "execute_process(COMMAND ${XDG_DESKTOP_MENU_EXECUTABLE} install --novendor share/juci.desktop)")
endif()
