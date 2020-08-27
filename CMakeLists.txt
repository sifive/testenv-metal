#------------------------------------------------------------------------------
# CMake test
#
# @file CMakeLists.txt
#------------------------------------------------------------------------------

# Import useful macros
CMAKE_MINIMUM_REQUIRED (VERSION 3.5)

IF (NOT DEFINED XBSP)
  MESSAGE (FATAL_ERROR "BSP is not selected; use -DXBSP=...")
ENDIF ()

IF (NOT IS_DIRECTORY ${CMAKE_SOURCE_DIR}/bsp/${XBSP})
  MESSAGE (FATAL_ERROR "Unsupported BSP ${XBSP}")
ENDIF ()

IF (NOT DEFINED CMAKE_BUILD_TYPE)
  MESSAGE (WARNING "Build type is not selected; use -DCMAKE_BUILD_TYPE=...")
ENDIF ()

SET (XISA imac)
STRING (REGEX REPLACE "^(qemu-.*-)([es])([0-9]+)$" "\\2" XSERIES ${XBSP})
IF (XSERIES STREQUAL "e")
  # 32-bit target
  SET (XLEN 32)
  # Unity test framework: enable 64-bit types on 32-bit platform
  ADD_DEFINITIONS (-DUNITY_SUPPORT_64)
ELSEIF (XSERIES STREQUAL "s")
  # 64-bit target
  SET (XLEN 64)
ELSE ()
  STRING (REGEX MATCH "^qemu-sifive_e_rv(32|64)$" bsp ${XBSP})
  MESSAGE (STATUS "BSP ${bsp}")
  IF (NOT bsp)
    MESSAGE (FATAL_ERROR "Unsupported BSP ${XBSP}")
  ELSE ()
    STRING (REGEX REPLACE "^(qemu-sifive_e_rv)(32|64)$" "\\2" XLEN ${XBSP})
  ENDIF ()
ENDIF ()

MESSAGE (STATUS "Select ${XLEN}-bit target")

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

#------------------------------------------------------------------------------
# List all subprojects, that is directories that contain a CMakeLists.txt file
# :outvar: output variable
#------------------------------------------------------------------------------
MACRO (find_subprojects outvar)
  SET (${outvar})
  FILE (GLOB subfiles
        LIST_DIRECTORIES true
        RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/*/CMakeLists.txt)
  SET (args ${ARGN})
  FOREACH (prj ${subfiles})
    GET_FILENAME_COMPONENT (subprj ${prj} DIRECTORY)
    IF (args)
      IF (${subprj} IN_LIST args)
        CONTINUE ()
      ENDIF ()
    ENDIF ()
    LIST (APPEND ${outvar} ${subprj})
  ENDFOREACH ()
ENDMACRO ()

#------------------------------------------------------------------------------
# Subprojects
#------------------------------------------------------------------------------
find_subprojects(subprojects)

FOREACH (prj ${subprojects})
  ADD_SUBDIRECTORY (${prj} ${prj})
ENDFOREACH ()
