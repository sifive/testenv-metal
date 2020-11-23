#------------------------------------------------------------------------------
# Define the build properties based on the selected BSP
# :global: XLEN (either 32 or 64)
# :global: XTARGET, the target triple
# :global: XARCH, rvXX
# :global: XABI
#------------------------------------------------------------------------------
MACRO (load_bsp_properties)
  IF (NOT DEFINED XBSP)
    MESSAGE (FATAL_ERROR "BSP is not selected; use -DXBSP=...")
  ENDIF ()
  IF (NOT IS_DIRECTORY ${CMAKE_SOURCE_DIR}/bsp/${XBSP})
    MESSAGE (FATAL_ERROR "Unsupported BSP ${XBSP}")
  ENDIF ()
  SET (XBSPDIR ${CMAKE_SOURCE_DIR}/bsp/${XBSP})
  INCLUDE_DIRECTORIES (${XBSPDIR}/include)
  FILE (STRINGS ${CMAKE_SOURCE_DIR}/bsp/${XBSP}/platform.def platdefs)
  FOREACH (platdef ${platdefs})
    STRING (FIND ${platdef} "=" eqsep)
    IF (NOT eqsep)
      MESSAGE (FATAL_ERROR "Invalid platform definition: ${platdef}")
    ENDIF ()
    STRING (SUBSTRING ${platdef} 0 ${eqsep} varname)
    STRING (STRIP ${varname} varname)
    MATH (EXPR eqsep ${eqsep}+1)
    STRING (SUBSTRING ${platdef} ${eqsep} -1 varval)
    STRING (STRIP ${varval} varval)
    SET (${varname} ${varval})
    ADD_DEFINITIONS(-D${varname}=${varval})
  ENDFOREACH ()
  SET (XTARGET riscv64-unknown-elf)
  IF (NOT DEFINED XLEN)
    MESSAGE (FATAL_ERROR "XLEN should be defined")
  ENDIF ()
  IF (NOT DEFINED XISA)
    MESSAGE (FATAL_ERROR "XISA should be defined")
  ENDIF ()
  SET (XARCH   rv${XLEN}${XISA})
  IF (XLEN EQUAL 64)
    SET (XABI  lp${XLEN})
  ELSE ()
    SET (XABI  ilp${XLEN})
  ENDIF ()
ENDMACRO ()

#-----------------------------------------------------------------------------
# Build and use SiFive metal framework
#-----------------------------------------------------------------------------
MACRO (enable_metal)
  SET (ENABLE_METAL 1)
  SET (METAL_SOURCE_DIR ${CMAKE_SOURCE_DIR}/freedom-metal)
  INCLUDE_DIRECTORIES (${METAL_SOURCE_DIR})
  #INCLUDE_DIRECTORIES (${METAL_SOURCE_DIR})
  INCLUDE_DIRECTORIES (${METAL_SOURCE_DIR}/sifive-blocks)
ENDMACRO ()

#-----------------------------------------------------------------------------
# Apply default link options to a final application
#  :app: the application component
#  :ld_script: the filename of the linker script
#-----------------------------------------------------------------------------
MACRO (link_application app ldscript)
  # libraries
  LIST (INSERT PROJECT_LINK_LIBRARIES 0 c clang_rt.builtins-riscv${XLEN})
  IF ( ENABLE_METAL )
    LIST (APPEND PROJECT_LINK_LIBRARIES metal metal-gloss)
  ENDIF ()
  LIST (APPEND PROJECT_LINK_LIBRARIES ${ARGN})
  IF ( ENABLE_UNITY )
    LIST (APPEND PROJECT_LINK_LIBRARIES unity)
  ENDIF ()
  # define the link options
  TARGET_LINK_LIBRARIES (${app}
                         # ${LDPREFIX}--warn-common
                         ${LDPREFIX}--gc-sections
                         ${LDPREFIX}--no-whole-archive
                         ${LDPREFIX}--warn-once
                         ${LDPREFIX}-static
                         --allow-multiple-definition
                         -T ${CMAKE_SOURCE_DIR}/bsp/${XBSP}/ld/${ldscript}
                         ${LINK_C_RUNTIME}
                         ${LDPREFIX}${LDSTARTGROUP}
                         ${LINK_SYSTEM_LIBS}
                         ${PROJECT_LINK_LIBRARIES}
                         ${LDPREFIX}${LDENDGROUP})
ENDMACRO ()
