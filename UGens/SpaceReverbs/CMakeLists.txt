cmake_minimum_required(VERSION 3.25)
project(DexterSpaceReverbs LANGUAGES CXX)
if(MSVC)
  # Keep the optional modules portable without a new VC runtime DLL install.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(SC_SOURCE_DIR "" CACHE PATH "Pinned SuperCollider headers")
set(SC3_PLUGINS_SOURCE_DIR "" CACHE PATH "sc3-plugins fa926d6b554acef35b2fd9deb2f996085fa86bd7")
if(NOT EXISTS "${SC_SOURCE_DIR}/include/plugin_interface/SC_PlugIn.h")
  message(FATAL_ERROR "SC_SOURCE_DIR must contain SuperCollider headers.")
endif()
set(DEIND "${SC3_PLUGINS_SOURCE_DIR}/source/DEINDUGens")
foreach(module IN ITEMS JPverb Greyhole)
  if(NOT EXISTS "${DEIND}/${module}Raw.cpp")
    message(FATAL_ERROR "Missing pinned upstream ${module}Raw.cpp")
  endif()
  # The upstream DSP and SC/Faust adapter are compiled unchanged. This mirrors
  # sc3-plugins' own target definitions without its other modules/dependencies.
  add_library(${module} MODULE "${DEIND}/${module}Raw.cpp")
  target_compile_features(${module} PRIVATE cxx_std_17)
  target_include_directories(${module} PRIVATE
    "${SC_SOURCE_DIR}/include/plugin_interface"
    "${SC_SOURCE_DIR}/include/common"
    "${SC_SOURCE_DIR}/common"
    "${DEIND}/include")
  string(TOUPPER "${module}" faust_class)
  target_compile_definitions(${module} PRIVATE
    SC_FAUST_PREFIX="" mydsp=${faust_class} NDEBUG _USE_MATH_DEFINES)
  set_target_properties(${module} PROPERTIES PREFIX "")
  if(WIN32)
    set_target_properties(${module} PROPERTIES SUFFIX ".scx")
  endif()
endforeach()
