#-----------------------------------------------------------------------------
# Definition file for Clang for RISC-V Metal targets
#-----------------------------------------------------------------------------

CMAKE_MINIMUM_REQUIRED (VERSION 3.5)

SET (CMAKE_SYSTEM_NAME metal)

IF (NOT DEFINED XLEN)
  MESSAGE (FATAL_ERROR "XLEN should be defined")
ENDIF ()

IF ( XLEN EQUAL 64 )
  SET (XMODEL medany)
ELSEIF ( XLEN EQUAL 32 )
  SET (XMODEL medlow)
ELSE ()
  MESSAGE (FATAL_ERROR "XLEN ${XLEN} not supported")
ENDIF ()

FIND_PROGRAM (xclang clang)
IF (NOT xclang)
  MESSAGE (FATAL_ERROR "Unable to locate clang compiler")
ENDIF ()

EXECUTE_PROCESS (COMMAND ${xclang} -print-target-triple
                 OUTPUT_VARIABLE CLANG_TRIPLE)

STRING (REGEX MATCH "riscv64-.*-elf" RISCV_ELF ${CLANG_TRIPLE})
IF (NOT RISCV_ELF)
  MESSAGE (FATAL_ERROR "${xclang} cannot build RISCV-ELF binaries")
ENDIF ()
STRING (REGEX REPLACE "(riscv64).*-(unknown-elf)\n?" "\\1-\\2"
        XTOOLCHAIN_TRIPLE ${CLANG_TRIPLE})

STRING (REPLACE "64" "${XLEN}" XTARGET ${XTOOLCHAIN_TRIPLE})

FIND_PROGRAM (ctidy NAMES clang-tidy)
FIND_PROGRAM (xar llvm-ar)
FIND_PROGRAM (xranlib llvm-ranlib)
FIND_PROGRAM (xobjdump llvm-objdump)
FIND_PROGRAM (xsize llvm-size)
FIND_PROGRAM (xnm llvm-nm)
FIND_PROGRAM (xobjcopy llvm-objcopy)
FIND_PROGRAM (xstrip llvm-strip)
FIND_PROGRAM (xld ${XTOOLCHAIN_TRIPLE}-ld)
FIND_PROGRAM (gnuxobjdump ${XTOOLCHAIN_TRIPLE}-objdump)

SET (xcc ${xclang})
SET (CMAKE_ASM_COMPILER_ID Clang)
SET (CMAKE_C_COMPILER_ID Clang)
SET (CMAKE_CXX_COMPILER_ID Clang)
SET (CMAKE_C_COMPILER_FORCED TRUE)
SET (CMAKE_CXX_COMPILER_FORCED TRUE)
SET (CMAKE_C_COMPILER ${xcc})
SET (CMAKE_CXX_COMPILER ${xcc})
SET (CMAKE_ASM_COMPILER ${xcc})
SET (CMAKE_AR ${xar})
SET (CMAKE_RANLIB ${xranlib})

SET (CMAKE_DEPFILE_FLAGS_C "-MD -MT <OBJECT> -MF <DEPFILE>")
SET (CMAKE_DEPFILE_FLAGS_CXX "-MD -MT <OBJECT> -MF <DEPFILE>")
SET (CMAKE_C_LINK_EXECUTABLE
       "${xld} <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

SET (X_FEAT_FLAGS
       "-ffunction-sections -fdata-sections")
SET (X_TARGET_FLAGS
       "-target ${XTARGET} -march=${XARCH} -mabi=${XABI} -mcmodel=${XMODEL}")
SET (X_EXTRA_FLAGS
       " -fdiagnostics-color=always -fansi-escape-codes")

SET (CMAKE_C_FLAGS "${X_FEAT_FLAGS} ${X_TARGET_FLAGS} ${X_EXTRA_FLAGS}")
SET (CMAKE_ASM_FLAGS "${X_FEAT_FLAGS} ${X_TARGET_FLAGS}")
SET (CMAKE_C_LINK_FLAGS "-b ${XTARGET} -nostartfiles -nostdlib --gc-sections")

SET (CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
SET (CMAKE_C_FLAGS_RELEASE "-O2 -Werror -ferror-limit=0 -DNDEBUG")

SET (XCC_SYSROOT "${XSYSROOT}/${XTARGET}/${XARCH}")
SET (CMAKE_C_STANDARD_INCLUDE_DIRECTORIES ${XCC_SYSROOT}/include)

# Use GNU linker w/o compiler frontend
#  ld.lld is not able to handle relaxation yet, so GNU LD is required for now
SET (LDPREFIX "")
SET (LDSTARTGROUP "--start-group")
SET (LDENDGROUP "--end-group")

LINK_DIRECTORIES (${XCC_SYSROOT}/lib)

FOREACH (xtool xar;xranlib;xobjdump;xsize;xnm;xobjcopy;xstrip)
  IF (NOT ${xtool})
    MESSAGE (FATAL_ERROR
      "Unable to locate a complete ${XTOOLCHAIN} C/C++ toolchain: ${xtool}")
  ENDIF ()
ENDFOREACH()
