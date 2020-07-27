INCLUDE_DIRECTORIES (src extras/fixture/src extras/memory/src)

ADD_LIBRARY (unity STATIC
  src/unity.c
  extras/fixture/src/unity_fixture.c
  extras/memory/src/unity_memory.c
)
