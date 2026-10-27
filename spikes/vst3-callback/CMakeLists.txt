cmake_minimum_required(VERSION 3.25)
project(DexterSCTonePlugin LANGUAGES C CXX VERSION 0.4.21)

set(VST3_SDK_ROOT "" CACHE PATH "Path to Steinberg VST3 SDK checkout")
set(SC_SOURCE_DIR "" CACHE PATH "Path to SuperCollider source checkout")
set(SC_BUILD_DIR "" CACHE PATH "Path to configured SuperCollider build")
set(SC_UGEN_PLUGIN_PATH "" CACHE PATH "Directory containing required SuperCollider UGen modules")
set(PYTHON_RUNTIME_DIR "" CACHE PATH "Directory containing the portable CPython embeddable runtime")
if(NOT EXISTS "${VST3_SDK_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "Set VST3_SDK_ROOT")
endif()
if(NOT EXISTS "${VST3_SDK_ROOT}/LICENSE.txt")
  message(FATAL_ERROR "VST3 SDK LICENSE.txt not found in VST3_SDK_ROOT")
endif()
if(NOT EXISTS "${SC_SOURCE_DIR}/include/server/SC_WorldOptions.h")
  message(FATAL_ERROR "Set SC_SOURCE_DIR")
endif()
if(NOT IS_DIRECTORY "${SC_BUILD_DIR}")
  message(FATAL_ERROR "Set SC_BUILD_DIR to the configured SuperCollider build")
endif()
if(NOT IS_DIRECTORY "${SC_UGEN_PLUGIN_PATH}")
  message(FATAL_ERROR "Set SC_UGEN_PLUGIN_PATH to the built SuperCollider UGen modules directory")
endif()

set(SC_UGEN_MODULE_FILES
  "${SC_UGEN_PLUGIN_PATH}/BinaryOpUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/ChaosUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/DelayUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/DemandUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/DemoUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/DynNoiseUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/FilterUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/GendynUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/GrainUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/IOUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/LFUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/MulAddUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/NoiseUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/OscUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/PanUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/PhysicalModelingUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/ReverbUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/TestUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/TriggerUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/UIUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/UnaryOpUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/UnpackFFTUGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/FFT_UGens.scx"
  "${SC_UGEN_PLUGIN_PATH}/PV_ThirdParty.scx"
  "${SC_UGEN_PLUGIN_PATH}/ML_UGens.scx")
foreach(SC_UGEN_MODULE_FILE IN LISTS SC_UGEN_MODULE_FILES)
  if(NOT EXISTS "${SC_UGEN_MODULE_FILE}")
    message(FATAL_ERROR "Required SuperCollider UGen module not found: ${SC_UGEN_MODULE_FILE}")
  endif()
endforeach()
if(NOT EXISTS "${SC_SOURCE_DIR}/COPYING")
  message(FATAL_ERROR "SuperCollider COPYING license file not found in SC_SOURCE_DIR")
endif()
if(NOT IS_DIRECTORY "${PYTHON_RUNTIME_DIR}")
  message(FATAL_ERROR "Set PYTHON_RUNTIME_DIR to the extracted Windows CPython 3.12 embeddable runtime")
endif()
foreach(PYTHON_REQUIRED_FILE python312.dll python3.dll python312.zip python312._pth
    vcruntime140.dll PYTHON-LICENSE.txt)
  if(NOT EXISTS "${PYTHON_RUNTIME_DIR}/${PYTHON_REQUIRED_FILE}")
    message(FATAL_ERROR "Required portable Python runtime file not found: ${PYTHON_RUNTIME_DIR}/${PYTHON_REQUIRED_FILE}")
  endif()
endforeach()

set(PYTHON_RUNTIME_FILES
  "${PYTHON_RUNTIME_DIR}/python312.dll"
  "${PYTHON_RUNTIME_DIR}/python3.dll"
  "${PYTHON_RUNTIME_DIR}/python312.zip"
  "${PYTHON_RUNTIME_DIR}/python312._pth")
foreach(PYTHON_VCRUNTIME_FILE vcruntime140.dll vcruntime140_1.dll)
  if(EXISTS "${PYTHON_RUNTIME_DIR}/${PYTHON_VCRUNTIME_FILE}")
    list(APPEND PYTHON_RUNTIME_FILES "${PYTHON_RUNTIME_DIR}/${PYTHON_VCRUNTIME_FILE}")
  endif()
endforeach()
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_BUNDLE_FOR_WINDOWS ON CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk")
set(SDK_ROOT "${VST3_SDK_ROOT}")
set(public_sdk_SOURCE_DIR "${VST3_SDK_ROOT}/public.sdk")
set(pluginterfaces_SOURCE_DIR "${VST3_SDK_ROOT}/pluginterfaces")

find_library(SC_LIBSCSYNTH NAMES scsynth libscsynth
  PATHS "${SC_BUILD_DIR}/server/scsynth"
  PATH_SUFFIXES Release RelWithDebInfo Debug
  NO_DEFAULT_PATH REQUIRED)
find_library(SC_TLSF NAMES tlsf
  PATHS "${SC_BUILD_DIR}/external_libraries"
  PATH_SUFFIXES Release RelWithDebInfo Debug
  NO_DEFAULT_PATH REQUIRED)
find_library(SC_PORTAUDIO NAMES portaudio portaudio_static
  PATHS "${SC_BUILD_DIR}/external_libraries/portaudio/portaudio_submodule"
  PATH_SUFFIXES Release RelWithDebInfo Debug
  NO_DEFAULT_PATH REQUIRED)

smtg_add_vst3plugin(dexter_sc_tone
  processor.cpp
  python_runtime.cpp
  win32_code_editor.cpp
  "${SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp")
smtg_target_add_plugin_resources(dexter_sc_tone
  OUTPUT_SUBDIRECTORY "Python"
  RESOURCES ${PYTHON_RUNTIME_FILES})
smtg_target_add_plugin_resources(dexter_sc_tone
  OUTPUT_SUBDIRECTORY "SCPlugins"
  RESOURCES ${SC_UGEN_MODULE_FILES})
smtg_target_add_plugin_resource(dexter_sc_tone "${SC_SOURCE_DIR}/COPYING" "Licenses")
smtg_target_add_plugin_resource(dexter_sc_tone "${CMAKE_CURRENT_SOURCE_DIR}/Licenses/Boost-1.0.txt" "Licenses")
smtg_target_add_plugin_resource(dexter_sc_tone "${VST3_SDK_ROOT}/LICENSE.txt" "Licenses")
smtg_target_add_plugin_resource(dexter_sc_tone "${PYTHON_RUNTIME_DIR}/PYTHON-LICENSE.txt" "Licenses")
smtg_target_add_plugin_resource(dexter_sc_tone "${CMAKE_CURRENT_SOURCE_DIR}/Licenses/FoxDot-CC-BY-SA-4.0.txt" "Licenses")
smtg_target_add_plugin_resource(dexter_sc_tone "${CMAKE_CURRENT_SOURCE_DIR}/Licenses/FoxDot-ATTRIBUTION.txt" "Licenses")
target_compile_features(dexter_sc_tone PRIVATE cxx_std_17)
target_include_directories(dexter_sc_tone PRIVATE
  "${SC_SOURCE_DIR}/include/common" "${SC_SOURCE_DIR}/common"
  "${SC_SOURCE_DIR}/include/server" "${SC_SOURCE_DIR}/include/plugin_interface"
  "${SC_SOURCE_DIR}/server/scsynth" "${SC_SOURCE_DIR}/external_libraries"
  "${SC_SOURCE_DIR}/external_libraries/nova-simd" "${SC_SOURCE_DIR}/external_libraries/nova-tt"
  "${SC_SOURCE_DIR}/external_libraries/boost_sync/include" "${SC_SOURCE_DIR}/external_libraries/boost"
  "${SC_SOURCE_DIR}/external_libraries/TLSF-2.4.6/src" "${CMAKE_CURRENT_SOURCE_DIR}"
  "${PYTHON_RUNTIME_DIR}" )
target_compile_definitions(dexter_sc_tone PRIVATE
  SC_AUDIO_API=SC_AUDIO_API_PORTAUDIO SC_MEMORY_ALIGNMENT=32 SC_FFT_GREEN NO_X11 NOVA_SIMD NO_LIBSNDFILE
  )
target_link_libraries(dexter_sc_tone PRIVATE sdk "${SC_LIBSCSYNTH}" "${SC_TLSF}" "${SC_PORTAUDIO}"
  ${CMAKE_DL_LIBS})
if(WIN32)
  target_link_libraries(dexter_sc_tone PRIVATE shell32 comdlg32 winmm ole32 uuid ws2_32)
elseif(UNIX AND NOT APPLE)
  target_link_libraries(dexter_sc_tone PRIVATE m rt pthread)
endif()


smtg_target_configure_version_file(dexter_sc_tone)
