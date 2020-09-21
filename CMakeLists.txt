#------------------------------------------------------------------------------
# Freedom-metal top level
#
# Syntax convention:
#  * UPPERCASE commands are CMake regular commands
#  * lowercase commands are local macros or functions
#------------------------------------------------------------------------------

CMAKE_MINIMUM_REQUIRED (VERSION 3.5)

# where to find CMake macros and configuration
SET (CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
# how to build for RISC-V baremetal targets
SET (CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/riscv.cmake)
# generate a JSON file with all build commands so that an IDE knows how to build
SET (CMAKE_EXPORT_COMPILE_COMMANDS=ON)

# SiFive macros
INCLUDE (macros)

# get target properties from the selected BSP
load_bsp_properties ()
# get the sysroot path from the host
define_xsysroot ()
# duplicate CMakeFiles.txt file into the freedom-metal and SCL submodules
deploy_cmakefiles ()

# The project to build
PROJECT (hca C ASM)

# build and use SiFive metal
enable_metal ()

# build Unity test framework
enable_unity ()

INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/scl-metal/include)

# Search directories with CMakeFiles.txt files, and load them
recurse_subprojects ()
