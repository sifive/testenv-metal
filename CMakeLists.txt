#------------------------------------------------------------------------------
# CMake test
#
# @file CMakeLists.txt
#------------------------------------------------------------------------------

# Import useful macros
CMAKE_MINIMUM_REQUIRED (VERSION 3.5)

IF (DEFINED XLEN AND XLEN EQUAL 64)
  # 64-bit target
  # SET (XLEN    64)
  SET (XISA    imac)
  SET (XBSP    qemu-arty-s51)
ELSE ()
  # 32-bit target
  SET (XLEN    32)
  SET (XISA    imac)
  SET (XBSP    qemu-arty-e31)
ENDIF ()

MESSAGE (STATUS "${XLEN}-bit target")

SET (CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
SET (CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/riscv.cmake)
SET (CMAKE_EXPORT_COMPILE_COMMANDS=ON)
# SET (CMAKE_BUILD_TYPE=DEBUG)  should be defined from command line
SET (XTARGET riscv64-unknown-elf)

SET (XARCH   rv${XLEN}${XISA})
IF (XLEN EQUAL 64)
  SET (XABI  lp${XLEN})
ELSE ()
  SET (XABI  ilp${XLEN})
ENDIF ()
SET (XSYSROOT /usr/local/opt/riscv${XLEN}-newlib)

PROJECT (hca C ASM)

INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/metal/include)
INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/bsp/${XBSP}/include)
INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/scl-metal/include)

ADD_SUBDIRECTORY (metal)
ADD_SUBDIRECTORY (gloss)
ADD_SUBDIRECTORY (scl-metal)
ADD_SUBDIRECTORY (tests)
