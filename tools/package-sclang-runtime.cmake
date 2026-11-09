if(NOT EXISTS "${SCLANG_EXE}")
  message(FATAL_ERROR "sclang executable not found: ${SCLANG_EXE}")
endif()
if(NOT IS_DIRECTORY "${SCLANG_SOURCE}/SCClassLibrary")
  message(FATAL_ERROR "SuperCollider class library not found: ${SCLANG_SOURCE}/SCClassLibrary")
endif()

file(MAKE_DIRECTORY "${SCLANG_OUTPUT}")
file(COPY_FILE "${SCLANG_EXE}" "${SCLANG_OUTPUT}/sclang.exe" ONLY_IF_DIFFERENT)
file(COPY "${SCLANG_SOURCE}/SCClassLibrary"
     DESTINATION "${SCLANG_OUTPUT}")

file(GLOB_RECURSE _runtime_dlls "${SCLANG_BUILD}/*.dll")
set(_runtime_directories)
foreach(_dll IN LISTS _runtime_dlls)
  get_filename_component(_directory "${_dll}" DIRECTORY)
  list(APPEND _runtime_directories "${_directory}")
endforeach()
list(REMOVE_DUPLICATES _runtime_directories)

file(GET_RUNTIME_DEPENDENCIES
  RESOLVED_DEPENDENCIES_VAR _resolved
  UNRESOLVED_DEPENDENCIES_VAR _unresolved
  EXECUTABLES "${SCLANG_OUTPUT}/sclang.exe"
  DIRECTORIES ${_runtime_directories}
  PRE_EXCLUDE_REGEXES
    "^api-ms-.*"
    "^ext-ms-.*"
)

if(_unresolved)
  message(FATAL_ERROR "Unresolved sclang runtime dependencies: ${_unresolved}")
endif()

file(TO_CMAKE_PATH "$ENV{SystemRoot}" _system_root)
string(TOLOWER "${_system_root}/" _system_root_prefix)
foreach(_dependency IN LISTS _resolved)
  string(TOLOWER "${_dependency}" _dependency_lower)
  string(FIND "${_dependency_lower}" "${_system_root_prefix}" _system_index)
  if(_system_index EQUAL 0)
    continue()
  endif()
  get_filename_component(_name "${_dependency}" NAME)
  file(COPY_FILE "${_dependency}" "${SCLANG_OUTPUT}/${_name}"
       ONLY_IF_DIFFERENT)
endforeach()

message(STATUS "Packaged sclang and ${_resolved} runtime dependencies.")
