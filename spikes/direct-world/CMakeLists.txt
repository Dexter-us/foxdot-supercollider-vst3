cmake_minimum_required(VERSION 3.22)
project(DexterSCWorldPoc LANGUAGES CXX)
include(CTest)

set(SC_SOURCE_DIR "" CACHE PATH "SuperCollider source checkout")
set(SC_BUILD_DIR "" CACHE PATH "Configured SuperCollider build directory")
if(NOT EXISTS "${SC_SOURCE_DIR}/include/server/SC_WorldOptions.h")
  message(FATAL_ERROR "Set SC_SOURCE_DIR to a SuperCollider source checkout")
endif()
if(NOT EXISTS "${SC_BUILD_DIR}/server/scsynth/libscsynth.a" AND NOT EXISTS "${SC_BUILD_DIR}/server/scsynth/libscsynth.lib")
  message(FATAL_ERROR "Set SC_BUILD_DIR to a built SuperCollider tree (libscsynth)")
endif()

find_package(Threads REQUIRED)
find_library(SC_LIBSCSYNTH NAMES scsynth libscsynth PATHS "${SC_BUILD_DIR}/server/scsynth" NO_DEFAULT_PATH REQUIRED)
find_library(SC_TLSF NAMES tlsf PATHS "${SC_BUILD_DIR}/external_libraries" NO_DEFAULT_PATH REQUIRED)
find_library(SC_PORTAUDIO NAMES portaudio portaudio_static PATHS "${SC_BUILD_DIR}/external_libraries/portaudio/portaudio_submodule" NO_DEFAULT_PATH REQUIRED)

add_executable(dexter_sc_world_poc main.cpp)
target_include_directories(dexter_sc_world_poc PRIVATE
  "${SC_SOURCE_DIR}/include/common"
  "${SC_SOURCE_DIR}/common"
  "${SC_SOURCE_DIR}/include/server"
  "${SC_SOURCE_DIR}/include/plugin_interface"
  "${SC_SOURCE_DIR}/server/scsynth"
  "${SC_SOURCE_DIR}/external_libraries"
  "${SC_SOURCE_DIR}/external_libraries/nova-simd"
  "${SC_SOURCE_DIR}/external_libraries/nova-tt"
  "${SC_SOURCE_DIR}/external_libraries/boost_sync/include"
  "${SC_SOURCE_DIR}/external_libraries/boost"
  "${SC_SOURCE_DIR}/external_libraries/TLSF-2.4.6/src")
target_compile_definitions(dexter_sc_world_poc PRIVATE
  SC_AUDIO_API=SC_AUDIO_API_PORTAUDIO
  SC_MEMORY_ALIGNMENT=32
  SC_FFT_GREEN
  NO_LIBSNDFILE
  NO_X11
  NOVA_SIMD)
target_link_libraries(dexter_sc_world_poc PRIVATE
  "${SC_LIBSCSYNTH}"
  "${SC_TLSF}"
  "${SC_PORTAUDIO}"
  Threads::Threads
  ${CMAKE_DL_LIBS})
if(UNIX AND NOT APPLE)
  target_link_libraries(dexter_sc_world_poc PRIVATE m rt)
endif()

add_test(NAME direct_sc_world_renders_audio
  COMMAND dexter_sc_world_poc
    "${CMAKE_CURRENT_SOURCE_DIR}/testdata/tone.scsyndef"
    "${SC_BUILD_DIR}/server/plugins")