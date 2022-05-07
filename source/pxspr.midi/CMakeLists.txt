cmake_minimum_required(VERSION 3.19)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in" "${CMAKE_BINARY_DIR}/version.h" @ONLY)
include_directories(${CMAKE_BINARY_DIR})

set(PXSPR_MIDI_COMMON_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(pxspr.midiin)
add_subdirectory(pxspr.midiinfo)
add_subdirectory(pxspr.midiout)
add_subdirectory(pxspr.sysexin)