# application name radix based on directory's name
GET_FILENAME_COMPONENT (COMPONENT ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# where to find unity header file
INCLUDE_DIRECTORIES (${CMAKE_SOURCE_DIR}/unity/src
                     ${CMAKE_SOURCE_DIR}/unity/extras/fixture/src
                     ${CMAKE_SOURCE_DIR}/unity/extras/memory/src)

# build a library with all the tests
ADD_LIBRARY (${COMPONENT}
  tests/api/aes/hardware/test_hca_aes_128.c
  tests/api/aes/hardware/test_hca_aes_192.c
  tests/api/aes/hardware/test_hca_aes_256.c
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
  tests/scl/sha/test_scl_sha_224.c
  tests/scl/sha/test_scl_sha_256.c
  tests/scl/sha/test_scl_sha_384.c
  tests/scl/sha/test_scl_sha_512.c
  tests/test_runners/test_hca_aes_runner.c
  tests/test_runners/test_hca_sha_runner.c
  tests/test_runners/test_scl_aes_runner.c
  tests/test_runners/test_scl_sha_runner.c
  tests/test_runners/test_soft_bignumbers_runner.c
  tests/test_runners/test_soft_sha_runner.c
  tests/test_runners/test_utils_runner.c
)

# libraries
LIST (INSERT PROJECT_LINK_LIBRARIES 0
      c clang_rt.builtins-riscv${XLEN} metal metal-gloss scl unity
      ${COMPONENT})

# which tests to build
SET (TESTS hca_sha hca_aes)

# build a test application for each test family
FOREACH (test ${TESTS})

  # build application named after the test kind
  STRING (REPLACE "_" "-" testname ${test})
  SET (testapp test-${COMPONENT}-${testname})
  ADD_EXECUTABLE (${testapp}
    test-scl-metal.c
  )

  # add a map file
  SET (mapfile "${CMAKE_CURRENT_BINARY_DIR}/${testapp}.map")
  SET_PROPERTY (TARGET ${testapp}
                APPEND
                PROPERTY LINK_FLAGS "${LDPREFIX}--Map ${LDPREFIX}${mapfile}")

  # select which tests function are used
  STRING (TOUPPER ${test} utest)
  SET_PROPERTY (TARGET ${testapp}
                APPEND
                PROPERTY COMPILE_DEFINITIONS TEST_${utest})

  # define the link options
  TARGET_LINK_LIBRARIES (${testapp}
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

  # disassemble the ELF file
  SET (xdisassemble
       ${gnuxobjdump} -dS)
  #SET (xdisassemble
  #     ${xobjdump} -disassemble -g -line-numbers -source)
  SET (app ${testapp}${CMAKE_EXECUTABLE_SUFFIX})
  ADD_CUSTOM_COMMAND (TARGET ${testapp} POST_BUILD
                      COMMAND ${xdisassemble}
                        ${app} 2> /dev/null > ${testapp}.S
                      COMMENT "Disassembling ELF file" VERBATIM)
  # get size info about the ELF file
  ADD_CUSTOM_COMMAND (TARGET ${testapp} POST_BUILD
                      COMMAND ${xsize}
                        ${app})
# each test application
ENDFOREACH ()
