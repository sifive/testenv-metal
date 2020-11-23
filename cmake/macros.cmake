#------------------------------------------------------------------------------
# Freedom-metal CMake macros
#
# @file macros.cmake
#------------------------------------------------------------------------------

IF (NOT DEFINED CMAKE_BUILD_TYPE)
  MESSAGE (WARNING "Build type is not selected; use -DCMAKE_BUILD_TYPE=...")
ENDIF ()

#------------------------------------------------------------------------------
# Define the sysroot directory based on the host type
# :global: XSYSROOT defined it not already set
#------------------------------------------------------------------------------
MACRO (define_xsysroot)
  IF (NOT DEFINED XSYSROOT)
    IF (NOT DEFINED XLEN)
      MESSAGE (FATAL_ERROR "XLEN is not yet defined")
    ENDIF ()
    IF (CMAKE_HOST_SYSTEM_NAME STREQUAL Darwin)
      # Assume a macOS Homebrew build
      SET (XSYSROOT /usr/local/opt/riscv${XLEN}-newlib)
    ELSEIF (CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
      # Assume a Docker build
      SET (XSYSROOT /usr/local/clang11)
    ELSE ()
      MESSAGE (FATAL_ERROR "Unknown host ${CMAKE_HOST_SYSTEM_NAME}")
    ENDIF ()
  ENDIF ()
  IF (NOT IS_DIRECTORY ${XSYSROOT})
    MESSAGE (FATAL_ERROR "Sysroot directory ${XSYSROOT} does not exist")
  ENDIF ()
ENDMACRO()

#-----------------------------------------------------------------------------
# Extract a parameter value from a list of argument.
# The parameter should be defined as key value pair: "PARAMETER value"
#  :outvar: output variable
#  :remargs: the input argument list w/o the removed argument
#  :parameter: the parameter name to look for
#  :*: list of arguments to parse
#-----------------------------------------------------------------------------
MACRO (extract_parameter outvar remargs parameter)
  SET (args ${ARGN})
  SET (${outvar})
  SET (${remargs})
  SET (match)
  SET (resume 1)
  FOREACH (arg ${args})
    IF (NOT resume)
      LIST (APPEND ${remargs} ${arg})
      CONTINUE ()
    ENDIF ()
    IF (match)
      # if the previous argument matched the seeked parameter
      # copy the current argument as the output value
      SET (${outvar} ${arg})
      # clear up the match flag
      SET (match)
      SET (resume)
    ELSE ()
      # the previous argument was not a match, try with the current one
      IF (${arg} STREQUAL ${parameter})
        # on match, flag a marker
        SET (match 1)
      ELSE ()
        LIST (APPEND ${remargs} ${arg})
      ENDIF ()
    # end of current argument loop
    ENDIF()
  ENDFOREACH ()
ENDMACRO ()

#-----------------------------------------------------------------------------
# Test if a parameter is defined in a list of argument.
#  :outvar: output variable
#  :remargs: the input argument list w/o the removed argument
#  :parameter: the parameter name to look for
#  :*: list of arguments to parse
# Note: do NOT use 'args' as the caller parameter variable name.
#-----------------------------------------------------------------------------
MACRO (test_parameter outvar remargs parameter)
  SET (args ${ARGN})
  SET (${outvar})
  SET (${remargs})
  FOREACH (arg ${args})
    STRING (COMPARE EQUAL ${arg} ${parameter} valdef)
    IF (valdef)
      SET (${outvar} ${valdef})
    ELSE ()
      LIST (APPEND ${remargs} ${arg})
    ENDIF ()
  ENDFOREACH ()
ENDMACRO ()

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
# Load all subprojects below the current directory
#------------------------------------------------------------------------------
MACRO (include_subprojects)
  find_subprojects (subprojects)
  FOREACH (prj ${subprojects})
    ADD_SUBDIRECTORY (${prj} ${prj})
  ENDFOREACH ()
ENDMACRO ()

#------------------------------------------------------------------------------
#
#------------------------------------------------------------------------------
MACRO (directory_name component)
  # application name radix based on directory's name
  GET_FILENAME_COMPONENT (${component} ${CMAKE_CURRENT_SOURCE_DIR} NAME)
ENDMACRO ()

#-----------------------------------------------------------------------------
# Enable most warnings
#-----------------------------------------------------------------------------
MACRO (enable_warnings_except warnings)
  # -Weverything is strongly discouraged, but here it easier to enable
  # all warnings and disable the onces that are not relevant
  ADD_DEFINITIONS ("-Weverything")
  SET (EXCEPT_BUILD "_")
  STRING (TOUPPER "${CMAKE_BUILD_TYPE}" UBUILD)
  FOREACH (warn ${ARGV})
    STRING (REGEX MATCH "^EXCEPT_([A-Z_]+)$" on_build ${warn})
    IF ( NOT on_build STREQUAL "" )
      STRING (REGEX REPLACE "^EXCEPT_" "" EXCEPT_BUILD ${warn})
    ELSE ()
      IF ( NOT "${UBUILD}" STREQUAL "${EXCEPT_BUILD}" )
        ADD_DEFINITIONS ("-Wno-${warn}")
      ENDIF ()
    ENDIF ()
  ENDFOREACH ()
ENDMACRO ()

#-----------------------------------------------------------------------------
# Conditionally enable clang static analyzer
#-----------------------------------------------------------------------------
MACRO (enable_static_analysis)
  IF ( STATIC_ANALYSIS )
    ADD_DEFINITIONS (
      "--analyze -Xanalyzer -analyzer-config "
      "-Xanalyzer -enable-checker=nullability.NullableDereferenced")
  ENDIF ()
ENDMACRO ()

#-----------------------------------------------------------------------------
# Build and use Unity unit test framework
#-----------------------------------------------------------------------------
MACRO (enable_unity)
  SET (ENABLE_UNITY 1)
  SET (UNITY_SOURCE_DIR ${CMAKE_SOURCE_DIR}/unity)
  INCLUDE_DIRECTORIES (${UNITY_SOURCE_DIR}/src
                       ${UNITY_SOURCE_DIR}/extras/fixture/src
                       ${UNITY_SOURCE_DIR}/extras/memory/src)
  # Do not use extras
  ADD_DEFINITIONS (-DUNITY_FIXTURE_NO_EXTRAS)
  IF ( XLEN EQUAL 32 )
    # Unity test framework: enable 64-bit types on 32-bit platform
    ADD_DEFINITIONS (-DUNITY_SUPPORT_64)
  ENDIF ()
  # Use ANSI color escape codes
  ADD_DEFINITIONS (-DUNITY_OUTPUT_COLOR)
ENDMACRO ()

#-----------------------------------------------------------------------------
# Exclude sub targets from the main ('all') target
#-----------------------------------------------------------------------------
MACRO (optional_target)
  IF (NOT DEFINED BUILD_OPTIONAL_TARGETS)
    SET_PROPERTY (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                  PROPERTY EXCLUDE_FROM_ALL ON)
  ENDIF ()
ENDMACRO ()

#-----------------------------------------------------------------------------
# Create a MAP file from an ELF file to dump symbols
#  :app: the application component
#  output file is generated within the same directory as the ELF file
#-----------------------------------------------------------------------------
MACRO (create_map_file app)
  SET (mapfile "${CMAKE_CURRENT_BINARY_DIR}/${app}.map")
  SET_PROPERTY (TARGET ${app}
                APPEND
                PROPERTY LINK_FLAGS "${LDPREFIX}--Map ${LDPREFIX}${mapfile}")
ENDMACRO ()

#-----------------------------------------------------------------------------
# Create byproducts from an application
#  :app: the application name
#  Optional parameters:
#    IHEX: Generate a Intel HEX output file
#    SREC: Generate a Motorola SREC output file
#    BIN: Generate a raw binary output file
#    ASM: Disassemble the application
#    SIZE: Report the size of the main application section
#-----------------------------------------------------------------------------
MACRO (post_gen_app app)
  SET (gen_args ${ARGN})
  test_parameter (gen_ihex doc_args "IHEX" ${gen_args})
  test_parameter (gen_srec doc_args "SREC" ${gen_args})
  test_parameter (gen_asm doc_args "ASM" ${gen_args})
  test_parameter (gen_bin doc_args "BIN" ${gen_args})
  test_parameter (gen_size doc_args "SIZE" ${gen_args})
  SET (appfile ${app}${CMAKE_EXECUTABLE_SUFFIX})
  IF (gen_asm)
    # disassemble the ELF file
    SET (xdisassemble
         ${gnuxobjdump} -dS)
    #SET (xdisassemble
    #     ${xobjdump} -disassemble -g -line-numbers -source)
    ADD_CUSTOM_COMMAND (TARGET ${app} POST_BUILD
                        COMMAND ${xdisassemble} ${DISASSEMBLE_OPTS}
                             ${appfile} > ${app}.S
                        COMMENT "Disassembling ELF file" VERBATIM)
  ENDIF ()
  IF (gen_size)
    ADD_CUSTOM_COMMAND (TARGET ${app} POST_BUILD
                        COMMAND ${xsize}
                             ${appfile})
  ENDIF ()
  IF (gen_ihex)
    ADD_CUSTOM_COMMAND (TARGET ${app} POST_BUILD
                        COMMAND ${xobjcopy}
                             -O ihex
                             ${appfile} ${app}.hex
                        COMMAND chmod -x ${app}.hex
                        COMMENT "Converting ELF to HEX" VERBATIM)
  ENDIF ()
  IF (gen_srec)
    ADD_CUSTOM_COMMAND (TARGET ${app} POST_BUILD
                        COMMAND ${xobjcopy}
                             -O srec
                             ${appfile} ${app}.srec
                        COMMAND chmod -x ${app}.srec
                        COMMENT "Converting ELF to SREC" VERBATIM)
  ENDIF ()
  IF (gen_bin)
    ADD_CUSTOM_COMMAND (TARGET ${app} POST_BUILD
                        COMMAND ${xobjcopy}
                             -O binary
                             ${appfile} ${app}.bin
                        COMMAND chmod -x ${app}.bin
                        COMMAND /bin/echo -n "   blob size: "
                        COMMAND ls -lh ${app}.bin | awk "{print \$5}"
                        COMMENT "Converting ELF to BIN" VERBATIM)
  ENDIF ()
ENDMACRO ()

#------------------------------------------------------------------------------
# Replicate CMakeFiles into the destination tree
# This is a hack for not storing CMake files into the actual repositories
#------------------------------------------------------------------------------
MACRO (deploy_cmakefiles)
  SET (_CMAKE_FILE_DIR ${CMAKE_SOURCE_DIR}/files)
  FILE (GLOB_RECURSE cmakefiles
        RELATIVE ${CMAKE_SOURCE_DIR}/cmake/files
        ${CMAKE_SOURCE_DIR}/cmake/files/*.txt)
  FOREACH (cmakefile ${cmakefiles})
    GET_FILENAME_COMPONENT (cmakedir ${cmakefile} DIRECTORY)
    IF (EXISTS ${CMAKE_SOURCE_DIR}/${cmakefile})
      FILE (SHA256 ${CMAKE_SOURCE_DIR}/cmake/files/${cmakefile} hsrc)
      FILE (SHA256 ${CMAKE_SOURCE_DIR}/${cmakefile} hdest)
      IF (NOT hsrc STREQUAL hdest)
        MESSAGE (STATUS "Override ${CMAKE_SOURCE_DIR}/${cmakefile}")
        FILE (COPY ${CMAKE_SOURCE_DIR}/cmake/files/${cmakefile}
              DESTINATION ${CMAKE_SOURCE_DIR}/${cmakedir})
      ENDIF ()
    ELSE ()
      IF (NOT EXISTS ${CMAKE_SOURCE_DIR}/${cmakefile})
        MESSAGE (STATUS "Create ${CMAKE_SOURCE_DIR}/${cmakefile}")
        FILE (COPY ${CMAKE_SOURCE_DIR}/cmake/files/${cmakefile}
              DESTINATION ${CMAKE_SOURCE_DIR}/${cmakedir})
      ENDIF ()
    ENDIF ()
  ENDFOREACH ()
ENDMACRO ()

#------------------------------------------------------------------------------
# Silent "Manually-specified variables were not used by the project" warnings
#------------------------------------------------------------------------------
SET (_IGNORE_ME "${BUILD_OPTIONAL_TARGETS}")
