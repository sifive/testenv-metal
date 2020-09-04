GET_FILENAME_COMPONENT (COMPONENT ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# where to find unity header file
INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/unity/src
                     ${CMAKE_SOURCE_DIR}/unity/extras/fixture/src
                     ${CMAKE_SOURCE_DIR}/unity/extras/memory/src)

ADD_EXECUTABLE (${COMPONENT}
                src/dma_aes_ecb.c
                src/dma_aes_gcm.c
                src/dma_sha256.c
                src/dma_sha512.c
                src/qemu.c
                src/secmain.S
                src/time.c
                src/trng.c)

LIST (INSERT PROJECT_LINK_LIBRARIES 0
      c clang_rt.builtins-riscv${XLEN} metal metal-gloss scl unity)

SET (mapfile "${CMAKE_CURRENT_BINARY_DIR}/${COMPONENT}.map")
GET_TARGET_PROPERTY (_link_flags ${COMPONENT}
                     LINK_FLAGS)
IF (NOT _link_flags)
  SET (_link_flags)
ENDIF ()
SET_TARGET_PROPERTIES (${COMPONENT} PROPERTIES LINK_FLAGS
  "${_link_flags} ${LDPREFIX}--Map ${LDPREFIX}${mapfile}")

TARGET_LINK_LIBRARIES (${COMPONENT}
                       # ${LDPREFIX}--warn-common
                       ${LDPREFIX}--gc-sections
                       ${LDPREFIX}--no-whole-archive
                       ${LDPREFIX}--warn-once
                       ${LDPREFIX}-static
                       --allow-multiple-definition
                       -T ${CMAKE_SOURCE_DIR}/bsp/${XBSP}/ld/metal.ld
                       ${LINK_C_RUNTIME}
                       ${LDPREFIX}${LDSTARTGROUP}
                       ${LINK_SYSTEM_LIBS}
                       ${PROJECT_LINK_LIBRARIES}
                       ${LDPREFIX}${LDENDGROUP})

#SET (xdisassemble
#     ${gnuxobjdump} -dS)
SET (xdisassemble
     ${xobjdump} -disassemble -g -line-numbers -source)

SET (app ${COMPONENT}${CMAKE_EXECUTABLE_SUFFIX})
ADD_CUSTOM_COMMAND (TARGET ${COMPONENT} POST_BUILD
                    COMMAND ${xdisassemble}
                      ${app} 2> /dev/null > ${COMPONENT}.S
                    COMMENT "Disassembling ELF file" VERBATIM)
ADD_CUSTOM_COMMAND (TARGET ${COMPONENT} POST_BUILD
                    COMMAND ${xsize}
                      ${app})
