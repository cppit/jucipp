#  LCL_FOUND - Libclangmm is available
#  LCL_INCLUDE_DIRS - The libclangmm include directories
#  LCL_LIBRARIES - 

find_package(PkgConfig)

find_path(LCL_INCLUDE_DIR clangmm.h
 PATH_SUFFIXES libclangmm
)

find_library(LCL_LIBRARY NAMES clangmm)

set(LCL_LIBRARIES ${LCL_LIBRARY} )
set(LCL_INCLUDE_DIRS ${LCL_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LCL DEFAULT_MSG
                                  LCL_LIBRARY LCL_INCLUDE_DIR)

mark_as_advanced(LCL_INCLUDE_DIR LCL_LIBRARY )
