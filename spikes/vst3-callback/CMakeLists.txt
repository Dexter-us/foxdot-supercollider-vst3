cmake_minimum_required(VERSION 3.25)
project(DexterSCTonePlugin LANGUAGES C CXX VERSION 0.1.0)

set(VST3_SDK_ROOT "" CACHE PATH "Path to Steinberg VST3 SDK checkout")
set(SC_SOURCE_DIR "" CACHE PATH "Path to SuperCollider source checkout")
set(SC_BUILD_DIR "" CACHE PATH "Path to configured SuperCollider build")
set(SC_UGEN_PLUGIN_PATH "" CACHE PATH "Directory containing required SuperCollider UGen modules")
if(NOT EXISTS "${VST3_SDK_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR "Set VST3_SDK_ROOT")
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

set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk")
set(SDK_ROOT "${VST3_SDK_ROOT}")
set(public_sdk_SOURCE_DIR "${VST3_SDK_ROOT}/public.sdk")
set(pluginterfaces_SOURCE_DIR "${VST3_SDK_ROOT}/pluginterfaces")

find_library(SC_LIBSCSYNTH NAMES scsynth libscsynth PATHS "${SC_BUILD_DIR}/server/scsynth" NO_DEFAULT_PATH REQUIRED)
find_library(SC_TLSF NAMES tlsf PATHS "${SC_BUILD_DIR}/external_libraries" NO_DEFAULT_PATH REQUIRED)
find_library(SC_PORTAUDIO NAMES portaudio portaudio_static PATHS "${SC_BUILD_DIR}/external_libraries/portaudio/portaudio_submodule" NO_DEFAULT_PATH REQUIRED)

smtg_add_vst3plugin(dexter_sc_tone processor.cpp "${SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp")
target_compile_features(dexter_sc_tone PRIVATE cxx_std_17)
target_include_directories(dexter_sc_tone PRIVATE
  "${SC_SOURCE_DIR}/include/common" "${SC_SOURCE_DIR}/common"
  "${SC_SOURCE_DIR}/include/server" "${SC_SOURCE_DIR}/include/plugin_interface"
  "${SC_SOURCE_DIR}/server/scsynth" "${SC_SOURCE_DIR}/external_libraries"
  "${SC_SOURCE_DIR}/external_libraries/nova-simd" "${SC_SOURCE_DIR}/external_libraries/nova-tt"
  "${SC_SOURCE_DIR}/external_libraries/boost_sync/include" "${SC_SOURCE_DIR}/external_libraries/boost"
  "${SC_SOURCE_DIR}/external_libraries/TLSF-2.4.6/src" "${CMAKE_CURRENT_SOURCE_DIR}")
file(TO_CMAKE_PATH "${SC_UGEN_PLUGIN_PATH}" SC_UGEN_PLUGIN_PATH_CMAKE)
target_compile_definitions(dexter_sc_tone PRIVATE
  SC_AUDIO_API=SC_AUDIO_API_PORTAUDIO SC_MEMORY_ALIGNMENT=32 SC_FFT_GREEN NO_LIBSNDFILE NO_X11 NOVA_SIMD
  SC_UGEN_PLUGIN_PATH="${SC_UGEN_PLUGIN_PATH_CMAKE}")
target_link_libraries(dexter_sc_tone PRIVATE sdk "${SC_LIBSCSYNTH}" "${SC_TLSF}" "${SC_PORTAUDIO}" ${CMAKE_DL_LIBS})
if(WIN32)
  target_link_libraries(dexter_sc_tone PRIVATE winmm ole32 uuid ws2_32)
elseif(UNIX AND NOT APPLE)
  target_link_libraries(dexter_sc_tone PRIVATE m rt pthread)
endif()
smtg_target_configure_version_file(dexter_sc_tone)
