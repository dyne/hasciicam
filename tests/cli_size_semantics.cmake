if(NOT DEFINED HASCIICAM_EXE)
  message(FATAL_ERROR "HASCIICAM_EXE is required")
endif()
if(NOT DEFINED HASCIICAM_CASE)
  message(FATAL_ERROR "HASCIICAM_CASE is required")
endif()

set(_stdout_file "${CMAKE_CURRENT_BINARY_DIR}/cli-size-${HASCIICAM_CASE}.out")
set(_stderr_file "${CMAKE_CURRENT_BINARY_DIR}/cli-size-${HASCIICAM_CASE}.err")

file(REMOVE "${_stdout_file}" "${_stderr_file}")

if(HASCIICAM_CASE STREQUAL "pixel")
  execute_process(
    COMMAND "${HASCIICAM_EXE}" -d synthetic:// --frames 2 --pixel-size 320x200 -m text -o "${_stdout_file}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_FILE "${_stderr_file}"
  )
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "pixel case failed rc=${_rc}")
  endif()
elseif(HASCIICAM_CASE STREQUAL "chars")
  execute_process(
    COMMAND "${HASCIICAM_EXE}" -d synthetic:// --frames 2 --char-size 80x25 -m text -o "${_stdout_file}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_FILE "${_stderr_file}"
  )
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "char case failed rc=${_rc}")
  endif()
elseif(HASCIICAM_CASE STREQUAL "html_short_s")
  execute_process(
    COMMAND "${HASCIICAM_EXE}" -d synthetic:// --frames 2 -m html -s 80x25 -o "${_stdout_file}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_FILE "${_stderr_file}"
  )
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "html short -s case failed rc=${_rc}")
  endif()
elseif(HASCIICAM_CASE STREQUAL "html_pixel_reject")
  execute_process(
    COMMAND "${HASCIICAM_EXE}" -d synthetic:// --frames 2 -m html --pixel-size 320x200 -o "${_stdout_file}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_FILE "${_stderr_file}"
  )
  if(_rc EQUAL 0)
    message(FATAL_ERROR "html pixel reject case unexpectedly succeeded")
  endif()
  file(READ "${_stderr_file}" _err)
  string(FIND "${_err}" "html mode does not accept pixel size" _has_msg)
  if(_has_msg EQUAL -1)
    message(FATAL_ERROR "missing html pixel rejection message")
  endif()
  return()
else()
  message(FATAL_ERROR "unknown HASCIICAM_CASE: ${HASCIICAM_CASE}")
endif()

if(NOT EXISTS "${_stdout_file}")
  message(FATAL_ERROR "expected output file not found: ${_stdout_file}")
endif()
file(READ "${_stdout_file}" _content)
string(LENGTH "${_content}" _len)
if(_len EQUAL 0)
  message(FATAL_ERROR "output is empty: ${_stdout_file}")
endif()
