#  LCL_FOUND - Libclangmm is available
#  LCL_INCLUDE_DIRS - The libclangmm include directories
#  LCL_LIBRARIES - 

find_package(PkgConfig)

if(MSYS)
  string(TOLOWER "/$ENV{MSYSTEM}" MSYS_PATH)
  set(MSYS_INCLUDE_PATH "${MSYS_PATH}/include/libclangmm")
  set(MSYS_BIN_PATH "${MSYS_PATH}/bin")
endif()

find_path(LCL_INCLUDE_DIR clangmm.h
  PATHS /usr/local/include/libclangmm ${MSYS_INCLUDE_PATH}
)

find_library(LCL_LIBRARY NAMES clangmm
  PATHS /usr/local/lib ${MSYS_BIN_PATH}
)

set(LCL_LIBRARIES ${LCL_LIBRARY} )
set(LCL_INCLUDE_DIRS ${LCL_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LCL DEFAULT_MSG
                                  LCL_LIBRARY LCL_INCLUDE_DIR)

mark_as_advanced(LCL_INCLUDE_DIR LCL_LIBRARY )
