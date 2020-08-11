find_subprojects(subprojects)

FOREACH (prj ${subprojects})
  ADD_SUBDIRECTORY (${prj} ${prj})
ENDFOREACH ()
