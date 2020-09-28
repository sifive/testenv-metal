IF ( ENABLE_UNITY )
  ADD_LIBRARY (unity
    src/unity.c
    extras/fixture/src/unity_fixture.c
    extras/memory/src/unity_memory.c
  )
ENDIF ()