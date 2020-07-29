# Only for SCL, there would be too many warning for the whole Metal
ADD_DEFINITIONS ("-Weverything")
# ADD_DEFINITIONS ("--analyze -Xanalyzer -analyzer-config "
#                  "-Xanalyzer -enable-checker=nullability.NullableDereferenced")

# Comment warnings in this list to get the associated error messages
LIST (APPEND DISABLE_WARNINGS
    reserved-id-macro
    newline-eof
    unreachable-code-break
    shorten-64-to-32
    cast-qual
    cast-align
    sign-conversion
    missing-variable-declarations
    covered-switch-default
    unreachable-code-return
    padded
    documentation
    vla
    language-extension-token
)

FOREACH (warn ${DISABLE_WARNINGS})
  ADD_DEFINITIONS ("-Wno-${warn}")
ENDFOREACH ()

ADD_LIBRARY (scl
  src/api/hardware/hca_utils.c
  src/api/hardware/v0.5/blockcipher/aes/hca_aes.c
  src/api/hardware/v0.5/hash/hca_sha.c
  src/api/hardware/v0.5/hash/hca_sha224.c
  src/api/hardware/v0.5/hash/hca_sha256.c
  src/api/hardware/v0.5/hash/hca_sha384.c
  src/api/hardware/v0.5/hash/hca_sha512.c
  src/api/hardware/v0.5/hash/hca_sha_miscellaneous.c
  src/api/hardware/v0.5/random/hca_trng.c
  src/api/software/bignumbers/soft_bignumbers.c
  # src/api/software/blockcipher/aes/soft_aes.c
  src/api/software/hash/soft_sha.c
  src/api/software/hash/soft_sha224.c
  src/api/software/hash/soft_sha256.c
  src/api/software/hash/soft_sha384.c
  src/api/software/hash/soft_sha512.c
  src/api/utils.c
  src/blockcipher/aes/scl_aes_cbc.c
  src/blockcipher/aes/scl_aes_ccm.c
  src/blockcipher/aes/scl_aes_cfb.c
  src/blockcipher/aes/scl_aes_ctr.c
  src/blockcipher/aes/scl_aes_ecb.c
  src/blockcipher/aes/scl_aes_gcm.c
  src/blockcipher/aes/scl_aes_ofb.c
  src/hash/sha/scl_sha.c
  src/scl_init.c
  src/scl_utils.c
)
