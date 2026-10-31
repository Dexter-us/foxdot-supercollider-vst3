cmake_minimum_required(VERSION 3.25)
project(DexterAirwindowsUGens LANGUAGES C CXX)

set(SC_SOURCE_DIR "" CACHE PATH "Pinned SuperCollider source tree (headers only)")
if(NOT EXISTS "${SC_SOURCE_DIR}/include/plugin_interface/SC_PlugIn.h")
  message(FATAL_ERROR "Set SC_SOURCE_DIR to the SuperCollider source tree.")
endif()

# These are the original Airwindows processor files, pinned to an immutable
# upstream commit and hash-checked before compilation. The adapter headers in
# this directory replace only the old VST2 class shell; DSP methods are built
# directly from these source files.
set(AIRWINDOWS_COMMIT "7fd5f5cae262a41e6dd7d9a83d60c46e84d3092a")
set(AIRWINDOWS_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/airwindows-source")
file(MAKE_DIRECTORY "${AIRWINDOWS_SOURCE_DIR}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Sources")

function(fetch_airwindows_processor filename path expected_sha256)
  set(destination "${AIRWINDOWS_SOURCE_DIR}/${filename}")
  file(DOWNLOAD
    "https://raw.githubusercontent.com/airwindows/airwindows/${AIRWINDOWS_COMMIT}/${path}"
    "${destination}"
    EXPECTED_HASH "SHA256=${expected_sha256}"
    TLS_VERIFY ON
    STATUS download_status
    SHOW_PROGRESS)
  list(GET download_status 0 download_code)
  list(GET download_status 1 download_message)
  if(NOT download_code EQUAL 0)
    file(REMOVE "${destination}")
    message(FATAL_ERROR "Could not fetch pinned Airwindows source ${filename}: ${download_message}")
  endif()
  set(${filename}_PATH "${destination}" PARENT_SCOPE)
endfunction()

fetch_airwindows_processor(
  "EchojamProc.cpp"
  "plugins/WinVST/Echojam/EchojamProc.cpp"
  "c3611b266cdf187b258bb50920e84b08979082837a0a38deca7fb045ff93eb4b")
fetch_airwindows_processor(
  "TapeDelay2Proc.cpp"
  "plugins/WinVST/TapeDelay2/TapeDelay2Proc.cpp"
  "a822cba0eb1c6c9e3f8eb835ad220699d132ccca9c0facfbba800d7739605c6e")

add_library(AirwindowsFX MODULE
  AirwindowsUGens.cpp
  "${EchojamProc.cpp_PATH}"
  "${TapeDelay2Proc.cpp_PATH}")
target_compile_features(AirwindowsFX PRIVATE cxx_std_17)
target_include_directories(AirwindowsFX PRIVATE
  "${SC_SOURCE_DIR}/include/plugin_interface"
  "${SC_SOURCE_DIR}/include/common"
  "${SC_SOURCE_DIR}/common"
  "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(AirwindowsFX PRIVATE _USE_MATH_DEFINES)
set_target_properties(AirwindowsFX PROPERTIES
  PREFIX ""
  SUFFIX ".scx"
  OUTPUT_NAME "AirwindowsFX")

add_custom_target(AirwindowsSources
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${EchojamProc.cpp_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/Sources/EchojamProc.cpp"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${TapeDelay2Proc.cpp_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/Sources/TapeDelay2Proc.cpp"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/Echojam.h" "${CMAKE_CURRENT_BINARY_DIR}/Sources/Echojam.h"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/TapeDelay2.h" "${CMAKE_CURRENT_BINARY_DIR}/Sources/TapeDelay2.h"
  COMMENT "Collecting the pinned Airwindows DSP source and UGen adapters")
