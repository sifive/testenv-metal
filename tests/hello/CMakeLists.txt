#------------------------------------------------------------------------------
# Simplest application to check an app can start and emit debug strings
#------------------------------------------------------------------------------

# Do not build with 'all' targets
optional_target ()

IF (NOT DEFINED STATIC_ANALYSIS)
  directory_name (COMPONENT)
  SET (app test-${COMPONENT})

  ADD_EXECUTABLE (${app}
    src/hello.c
  )

  link_application (${app} metal.ld
                    metal metal-gloss)

  create_map_file(${app})
  post_gen_app(${app} ASM SIZE)
ENDIF ()
