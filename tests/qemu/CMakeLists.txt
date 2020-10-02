#------------------------------------------------------------------------------
# HCA tests for QEMU VM
# the following features are not yet exercized in regular scl-metal-tests
#------------------------------------------------------------------------------

directory_name (component)

# where to find unity header file
INCLUDE_DIRECTORIES (include
                     ${CMAKE_SOURCE_DIR}/unity/src
                     ${CMAKE_SOURCE_DIR}/unity/extras/fixture/src
                     ${CMAKE_SOURCE_DIR}/unity/extras/memory/src)

IF (NOT DEFINED STATIC_ANALYSIS)
  SET (app test-${component})

  ADD_EXECUTABLE (${app}
     src/dma_aes_ecb.c
     src/dma_aes_gcm.c
     src/dma_sha256.c
     src/dma_sha512.c
     src/qemu.c
     src/secmain.S
     src/time.c
     src/trng.c
  )
  link_application (${app} metal.ld scl)

  create_map_file(${app})
  post_gen_app(${app} ASM SIZE)
ENDIF ()
