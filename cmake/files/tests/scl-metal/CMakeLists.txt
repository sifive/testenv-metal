#------------------------------------------------------------------------------
# SiFive cryptographic library unit tests
#------------------------------------------------------------------------------

# name the library from the current directory
directory_name (component)

# where to find unity header file
INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/unity/src
                     ${CMAKE_SOURCE_DIR}/unity/extras/fixture/src
                     ${CMAKE_SOURCE_DIR}/unity/extras/memory/src)

# build a static library with all the tests
#  this enables running the static analyzer on all the test files, w/o to try
#  to produce an executable
ADD_LIBRARY (${component}
  tests/api/aes/hardware/test_hca_aes_128.c
  tests/api/aes/hardware/test_hca_aes_192.c
  tests/api/aes/hardware/test_hca_aes_256.c
  tests/api/asymmetric/ecc/software/test_soft_ecc.c
  tests/api/asymmetric/ecc/software/test_soft_ecdsa.c
  tests/api/bignumbers/software/test_soft_bignumbers.c
  tests/api/sha/hardware/test_hca_sha_224.c
  tests/api/sha/hardware/test_hca_sha_256.c
  tests/api/sha/hardware/test_hca_sha_384.c
  tests/api/sha/hardware/test_hca_sha_512.c
  tests/api/sha/software/test_soft_sha_224.c
  tests/api/sha/software/test_soft_sha_256.c
  tests/api/sha/software/test_soft_sha_384.c
  tests/api/sha/software/test_soft_sha_512.c
  tests/api/test_utils.c
  tests/scl/aes/test_scl_aes_128.c
  tests/scl/aes/test_scl_aes_192.c
  tests/scl/aes/test_scl_aes_256.c
  tests/scl/asymmetric/ecc/test_soft_ecdsa.c
  tests/scl/sha/test_scl_sha_224.c
  tests/scl/sha/test_scl_sha_256.c
  tests/scl/sha/test_scl_sha_384.c
  tests/scl/sha/test_scl_sha_512.c
  tests/test_runners/test_hca_aes_runner.c
  tests/test_runners/test_hca_sha_runner.c
  tests/test_runners/test_scl_aes_runner.c
  tests/test_runners/test_scl_ecdsa_runner.c
  tests/test_runners/test_scl_sha_runner.c
  tests/test_runners/test_soft_bignumbers_runner.c
  tests/test_runners/test_soft_ecc_runner.c
  tests/test_runners/test_soft_ecdsa_runner.c
  tests/test_runners/test_soft_sha_runner.c
  tests/test_runners/test_utils_runner.c
)

IF (NOT DEFINED STATIC_ANALYSIS)
  SET (app test-${component})

  ADD_EXECUTABLE (${app}
    test-scl-metal.c
  )

  link_application (${app} metal.ld
                    metal metal-gloss scl unity ${component})

  create_map_file(${app})
  # generate artifacts: de-assembled binary and report the binary size info
  post_gen_app(${app} ASM SIZE)
ENDIF ()