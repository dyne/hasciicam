if(NOT DEFINED HASCIICAM_EXE)
  message(FATAL_ERROR "HASCIICAM_EXE is required")
endif()
if(NOT DEFINED HASCIICAM_OUTPUT_DIR)
  message(FATAL_ERROR "HASCIICAM_OUTPUT_DIR is required")
endif()

set(_config_file "${HASCIICAM_OUTPUT_DIR}/cli-startup.toml")
set(_output_file "${HASCIICAM_OUTPUT_DIR}/cli-config-startup.asc")
set(_stderr_file "${HASCIICAM_OUTPUT_DIR}/cli-config-startup.err")

file(REMOVE "${_config_file}" "${_output_file}" "${_stderr_file}")
file(WRITE "${_config_file}" "mode = \"text\"\n")
file(APPEND "${_config_file}" "output_file = \"${_output_file}\"\n")
file(APPEND "${_config_file}" "device = \"synthetic://\"\n")
file(APPEND "${_config_file}" "frames = 2\n")

execute_process(
  COMMAND "${HASCIICAM_EXE}" --config "${_config_file}"
  RESULT_VARIABLE _rc
  OUTPUT_VARIABLE _out
  ERROR_FILE "${_stderr_file}"
)
if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "hasciicam --config exited with ${_rc}")
endif()

if(NOT EXISTS "${_output_file}")
  message(FATAL_ERROR "configured output file not found: ${_output_file}")
endif()
file(READ "${_output_file}" _content)
string(REGEX MATCH "[^ \t\r\n]" _nonws "${_content}")
if(NOT _nonws)
  message(FATAL_ERROR "configured output has no visible content")
endif()
