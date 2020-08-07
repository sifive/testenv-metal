#------------------------------------------------------------------------------
# CMake test
#
# @file CMakeLists.txt
#------------------------------------------------------------------------------

# Import useful macros
CMAKE_MINIMUM_REQUIRED (VERSION 3.5)

IF (NOT DEFINED XBSP)
  SET (XBSP qemu-arty-s51)
ENDIF ()

IF (NOT IS_DIRECTORY ${CMAKE_SOURCE_DIR}/bsp/${XBSP})
  MESSAGE (FATAL_ERROR "Unsupported BSP ${XBSP}")
ENDIF ()

STRING (REGEX REPLACE "^(qemu-.*-)([es])([0-9]+)$" "\\2" XSERIES ${XBSP})
IF (XSERIES STREQUAL "e")
  # 32-bit target
  SET (XLEN    32)
  SET (XISA    imac)
ELSEIF (XSERIES STREQUAL "s")
  # 64-bit target
  SET (XLEN    64)
  SET (XISA    imac)
ELSE ()
  MESSAGE (FATAL_ERROR "Unsupported XBSP")
ENDIF ()

MESSAGE (STATUS "Select ${XLEN}-bit target")
MESSAGE (STATUS "Select ${XBSP} BSP")

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

IF (CMAKE_HOST_SYSTEM_NAME STREQUAL Darwin)
  # Assume a Homebrew build
  SET (XSYSROOT /usr/local/opt/riscv${XLEN}-newlib)
ELSEIF (CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
  # Assume a Docker build
  SET (XSYSROOT /usr/local/clang10)
ELSE ()
  MESSAGE (FATAL_ERROR "Unknown host ${CMAKE_HOST_SYSTEM_NAME}")
ENDIF ()

SET (_CMAKE_FILE_DIR ${CMAKE_SOURCE_DIR}/files)
FILE (GLOB_RECURSE cmakefiles
      RELATIVE ${CMAKE_SOURCE_DIR}/cmake/files
      # CONFIGURE_DEPENDS
      ${CMAKE_SOURCE_DIR}/cmake/files/*.txt)

PROJECT (hca C ASM)

INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/metal/include)
INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/bsp/${XBSP}/include)
INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/scl-metal/include)

FOREACH (cmakefile ${cmakefiles})
  GET_FILENAME_COMPONENT (cmakedir ${cmakefile} DIRECTORY)
  # MESSAGE (STATUS "CMAKE ${CMAKE_SOURCE_DIR}/cmake/files/${cmakefile}
  #          ${CMAKE_SOURCE_DIR}/${cmakedir}")
  IF (NOT EXISTS ${CMAKE_SOURCE_DIR}/${cmakefile})
    FILE (COPY ${CMAKE_SOURCE_DIR}/cmake/files/${cmakefile}
          DESTINATION ${CMAKE_SOURCE_DIR}/${cmakedir})
  ENDIF ()
ENDFOREACH ()

ADD_SUBDIRECTORY (metal)
ADD_SUBDIRECTORY (unity)
ADD_SUBDIRECTORY (gloss)
ADD_SUBDIRECTORY (scl-metal)
ADD_SUBDIRECTORY (tests)
