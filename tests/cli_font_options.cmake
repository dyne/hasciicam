if(NOT DEFINED HASCIICAM_EXE)
  message(FATAL_ERROR "HASCIICAM_EXE is required")
endif()
if(NOT DEFINED HASCIICAM_OUTPUT_DIR)
  message(FATAL_ERROR "HASCIICAM_OUTPUT_DIR is required")
endif()

set(_list_file "${HASCIICAM_OUTPUT_DIR}/cli-font-list.out")
set(_list_err "${HASCIICAM_OUTPUT_DIR}/cli-font-list.err")
set(_text_out "${HASCIICAM_OUTPUT_DIR}/cli-font-vga8.asc")
set(_text_err "${HASCIICAM_OUTPUT_DIR}/cli-font-vga8.err")
file(REMOVE "${_list_file}" "${_list_err}" "${_text_out}" "${_text_err}")

execute_process(
  COMMAND "${HASCIICAM_EXE}" --font list
  RESULT_VARIABLE _list_rc
  OUTPUT_FILE "${_list_file}"
  ERROR_FILE "${_list_err}"
)
if(NOT _list_rc EQUAL 0)
  message(FATAL_ERROR "--font list exited with ${_list_rc}")
endif()
file(READ "${_list_file}" _list_text)
string(FIND "${_list_text}" "vga16\t" _has_vga16)
if(_has_vga16 EQUAL -1)
  message(FATAL_ERROR "--font list output missing vga16")
endif()
string(FIND "${_list_text}" "courier\t" _has_courier)
if(_has_courier EQUAL -1)
  message(FATAL_ERROR "--font list output missing courier")
endif()

execute_process(
  COMMAND "${HASCIICAM_EXE}" -d synthetic:// --frames 2 -m text --font vga8 -o "${_text_out}"
  RESULT_VARIABLE _text_rc
  OUTPUT_VARIABLE _text_stdout
  ERROR_FILE "${_text_err}"
)
if(NOT _text_rc EQUAL 0)
  message(FATAL_ERROR "--font vga8 text run exited with ${_text_rc}")
endif()
if(NOT EXISTS "${_text_out}")
  message(FATAL_ERROR "expected output file not found: ${_text_out}")
endif()
file(READ "${_text_out}" _content)
string(REGEX MATCH "[^ \t\r\n]" _nonws "${_content}")
if(NOT _nonws)
  message(FATAL_ERROR "font-selected output has no visible content")
endif()
