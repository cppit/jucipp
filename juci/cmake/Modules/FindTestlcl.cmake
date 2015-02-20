# - Try to find LCL


# Once done this will define
#  LCL_FOUND - System has LibXml2
#  LCL_INCLUDE_DIRS - The LibXml2 include directories
#  LCL_LIBRARIES - The libraries needed to use LibXml2
#  LCL_DEFINITIONS - Compiler switches required for using LibXml2

find_package(PkgConfig)

find_path(LCL_INCLUDE_DIR headers/TranslationUnit.h
          HINTS "/home/zalox/bachelor/libclang++/"
          )

find_library(LCL_LIBRARY NAMES testlcl
  HINTS  "/home/zalox/bachelor/libclang++/lib")

set(LCL_LIBRARIES ${LCL_LIBRARY} )
set(LCL_INCLUDE_DIRS ${LCL_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBXML2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LCL DEFAULT_MSG
                                  LCL_LIBRARY LCL_INCLUDE_DIR)

mark_as_advanced(LCL_INCLUDE_DIR LCL_LIBRARY )