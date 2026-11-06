# Writes OUT with TINYLINK_VERSION from `git describe` (leading "v" stripped),
# only when it changed, so an unchanged version doesn't trigger a rebuild.
# Run with -DGIT=<git> -DSRC=<repo dir> -DOUT=<header>.
set(version "")
if(GIT)
  execute_process(COMMAND "${GIT}" describe --tags --always --dirty
                  WORKING_DIRECTORY "${SRC}"
                  OUTPUT_VARIABLE version
                  RESULT_VARIABLE result
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  ERROR_QUIET)
  if(result)
    set(version "")
  endif()
endif()
if(NOT version)
  set(version "0.0.0-unknown")
endif()
string(REGEX REPLACE "^v" "" version "${version}")

set(content "#pragma once\n\n// Generated from `git describe` at build time.\n#define TINYLINK_VERSION \"${version}\"\n")
set(old "")
if(EXISTS "${OUT}")
  file(READ "${OUT}" old)
endif()
if(NOT old STREQUAL content)
  file(WRITE "${OUT}" "${content}")
endif()
