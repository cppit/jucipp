# Files used both in ../src and ../tests
set(JUCI_SHARED_FILES
  autocomplete.cc
  cmake.cc
  compile_commands.cc
  ctags.cc
  dispatcher.cc
  documentation_cppreference.cc
  filesystem.cc
  git.cc
  menu.cc
  meson.cc
  project_build.cc
  source.cc
  source_base.cc
  source_clang.cc
  source_diff.cc
  source_language_protocol.cc
  source_spellcheck.cc
  terminal.cc
  usages_clang.cc
)
if(LIBLLDB_FOUND)
  list(APPEND JUCI_SHARED_FILES debug_lldb.cc)
endif()
add_library(juci_shared STATIC ${JUCI_SHARED_FILES})
target_link_libraries(juci_shared
  ${GTKMM_LIBRARIES}
  ${GTKSVMM_LIBRARIES}
  ${Boost_LIBRARIES}
  ${LIBLLDB_LIBRARIES}
  ${ASPELL_LIBRARIES}
  ${LIBGIT2_LIBRARIES}
  clangmm
  tiny-process-library
)

add_executable(juci
  config.cc
  dialogs.cc
  dialogs_unix.cc
  directories.cc
  entrybox.cc
  info.cc
  juci.cc
  notebook.cc
  project.cc
  selection_dialog.cc
  tooltips.cc
  window.cc
)
target_link_libraries(juci juci_shared)

install(TARGETS juci RUNTIME DESTINATION bin)
if(${CMAKE_SYSTEM_NAME} MATCHES Linux|.*BSD|DragonFly)
  install(FILES "${CMAKE_SOURCE_DIR}/share/juci.desktop"
          DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
  install(FILES "${CMAKE_SOURCE_DIR}/share/juci.svg"
          DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps")
elseif(APPLE)
  install(CODE "execute_process(COMMAND /usr/bin/python ${CMAKE_SOURCE_DIR}/share/set_icon_macos.py ${CMAKE_SOURCE_DIR}/share/juci.png ${CMAKE_INSTALL_PREFIX}/bin/juci)")
endif()

# add a target to generate API documentation with Doxygen
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules/")
find_package(Plantuml)
find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen to ${CMAKE_CURRENT_BINARY_DIR}" VERBATIM
  )
endif(DOXYGEN_FOUND)
