if(NOT DEFINED HASCIICAM_EXE)
  message(FATAL_ERROR "HASCIICAM_EXE is required")
endif()
if(NOT DEFINED HASCIICAM_MODE)
  message(FATAL_ERROR "HASCIICAM_MODE is required")
endif()
if(NOT DEFINED HASCIICAM_OUTPUT)
  message(FATAL_ERROR "HASCIICAM_OUTPUT is required")
endif()

set(_stdout_file "${HASCIICAM_OUTPUT}")
set(_stderr_file "${HASCIICAM_OUTPUT}.err")

file(REMOVE "${_stdout_file}" "${_stderr_file}")

if(HASCIICAM_MODE STREQUAL "stdout")
  execute_process(
    COMMAND "${HASCIICAM_EXE}" -d synthetic:// --frames 2 -O stdout
    RESULT_VARIABLE _rc
    OUTPUT_FILE "${_stdout_file}"
    ERROR_FILE "${_stderr_file}"
  )
elseif(HASCIICAM_MODE STREQUAL "text")
  execute_process(
    COMMAND "${HASCIICAM_EXE}" -d synthetic:// --frames 2 -m text -o "${_stdout_file}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_FILE "${_stderr_file}"
  )
elseif(HASCIICAM_MODE STREQUAL "html")
  execute_process(
    COMMAND "${HASCIICAM_EXE}" -d synthetic:// --frames 2 -m html -o "${_stdout_file}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_FILE "${_stderr_file}"
  )
else()
  message(FATAL_ERROR "Unknown HASCIICAM_MODE: ${HASCIICAM_MODE}")
endif()

if(NOT _rc EQUAL 0)
  message(FATAL_ERROR "hasciicam exited with ${_rc}")
endif()

if(NOT EXISTS "${_stdout_file}")
  message(FATAL_ERROR "expected output file not found: ${_stdout_file}")
endif()

file(READ "${_stdout_file}" _content)
string(LENGTH "${_content}" _len)
if(_len EQUAL 0)
  message(FATAL_ERROR "output is empty: ${_stdout_file}")
endif()

string(REGEX MATCH "[^ \t\r\n]" _nonws "${_content}")
if(NOT _nonws)
  message(FATAL_ERROR "output has no visible content: ${_stdout_file}")
endif()

if(HASCIICAM_MODE STREQUAL "html")
  string(FIND "${_content}" "HTTP-EQUIV=\"refresh\"" _has_refresh)
  if(_has_refresh EQUAL -1)
    message(FATAL_ERROR "html output missing refresh tag")
  endif()
  string(FIND "${_content}" "<PRE>" _has_pre)
  if(_has_pre EQUAL -1)
    message(FATAL_ERROR "html output missing PRE tag")
  endif()
endif()
