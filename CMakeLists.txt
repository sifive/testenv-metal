#------------------------------------------------------------------------------
# Freedom-metal top level
#
# Syntax convention:
#  * UPPERCASE commands are CMake regular commands
#  * lowercase commands are local macros or functions
#------------------------------------------------------------------------------

CMAKE_MINIMUM_REQUIRED (VERSION 3.5)

SET (CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
SET (CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/riscv.cmake)
SET (CMAKE_EXPORT_COMPILE_COMMANDS=ON)
INCLUDE (macros)

load_bsp_properties ()
define_xsysroot ()
deploy_cmakefiles ()

PROJECT (hca C ASM)

INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/bsp/${XBSP}/include
                     ${CMAKE_SOURCE_DIR}/metal/include
                     ${CMAKE_SOURCE_DIR}/scl-metal/include)

recurse_subprojects ()
