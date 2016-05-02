if(NOT DEFINED plantuml_FOUND)
   find_file(PLANTUML_JARFILE
     NAMES plantuml.jar
     HINTS "$ENV{PLANTUML_PATH}" ENV PLANTUML_DIR
   )
   include(FindPackageHandleStandardArgs)
   find_package_handle_standard_args(
     plantuml DEFAULT_MSG PLANTUML_JARFILE)
endif()
