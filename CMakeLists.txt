#------------------------------------------------------------------------------
# CMake test
#
# @file CMakeLists.txt
#------------------------------------------------------------------------------

# Import useful macros
CMAKE_MINIMUM_REQUIRED (VERSION 3.5)

SET (XSYSROOT /usr/local/opt/riscv32-newlib)
SET (CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
SET (CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/riscv.cmake)
# SET (CMAKE_EXPORT_COMPILE_COMMANDS=ON)
SET (CMAKE_BUILD_TYPE=DEBUG)

SET (XTARGET riscv32-unknown-elf)
SET (XARCH   rv32imac)
SET (XABI    ilp32)
SET (XBSP    qemu-arty-e31)

PROJECT (hca C ASM)

INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/metal/include)
INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/bsp/${XBSP}/include)

ADD_SUBDIRECTORY (metal)
ADD_SUBDIRECTORY (gloss)
ADD_SUBDIRECTORY (tests)
