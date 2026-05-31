if(NOT DEFINED HASCIICAM_EXE)
  message(FATAL_ERROR "HASCIICAM_EXE is required")
endif()
if(NOT DEFINED HASCIICAM_OUTPUT_DIR)
  message(FATAL_ERROR "HASCIICAM_OUTPUT_DIR is required")
endif()

set(_low_file "${HASCIICAM_OUTPUT_DIR}/cli-tuning-low.txt")
set(_high_file "${HASCIICAM_OUTPUT_DIR}/cli-tuning-high.txt")
set(_stderr_file "${HASCIICAM_OUTPUT_DIR}/cli-tuning.err")

file(REMOVE "${_low_file}" "${_high_file}" "${_stderr_file}")

execute_process(
  COMMAND "${HASCIICAM_EXE}" -d synthetic:// --frames 2 -m text -o "${_low_file}" -b 0
  RESULT_VARIABLE _low_rc
  OUTPUT_VARIABLE _low_out
  ERROR_FILE "${_stderr_file}"
)
if(NOT _low_rc EQUAL 0)
  message(FATAL_ERROR "low-brightness render exited with ${_low_rc}")
endif()

execute_process(
  COMMAND "${HASCIICAM_EXE}" -d synthetic:// --frames 2 -m text -o "${_high_file}" -b 120
  RESULT_VARIABLE _high_rc
  OUTPUT_VARIABLE _high_out
  ERROR_FILE "${_stderr_file}"
)
if(NOT _high_rc EQUAL 0)
  message(FATAL_ERROR "high-brightness render exited with ${_high_rc}")
endif()

file(READ "${_low_file}" _low_content)
file(READ "${_high_file}" _high_content)

if(_low_content STREQUAL _high_content)
  message(FATAL_ERROR "AA brightness did not change rendered text output")
endif()
