cmake_minimum_required(VERSION 3.25)
project(DexterDiskIOUGens LANGUAGES C CXX)
set(SC_SOURCE_DIR "" CACHE PATH "SuperCollider ef5771b source tree; no engine build needed")
if(NOT EXISTS "${SC_SOURCE_DIR}/server/plugins/DiskIO_UGens.cpp")
  message(FATAL_ERROR "Set SC_SOURCE_DIR to pinned SuperCollider sources.")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
include(FetchContent)
# Shared LGPL library, replaceable by the user. No FLAC/Vorbis/Opus/MPEG deps.
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ENABLE_EXTERNAL_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_MPEG OFF CACHE BOOL "" FORCE)
set(ENABLE_CPACK OFF CACHE BOOL "" FORCE)
set(ENABLE_PACKAGE_CONFIG OFF CACHE BOOL "" FORCE)
FetchContent_Declare(sndfile
  URL https://codeload.github.com/libsndfile/libsndfile/tar.gz/1.2.2
  URL_HASH SHA256=ffe12ef8add3eaca876f04087734e6e8e029350082f3251f565fa9da55b52121)
FetchContent_MakeAvailable(sndfile)
set(generated "${CMAKE_CURRENT_BINARY_DIR}/DiskIO_UGens.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare-source.py"
  "${SC_SOURCE_DIR}/server/plugins/DiskIO_UGens.cpp" "${generated}"
  COMMAND_ERROR_IS_FATAL ANY)
add_library(DiskIO_UGens MODULE "${generated}")
target_compile_features(DiskIO_UGens PRIVATE cxx_std_17)
target_include_directories(DiskIO_UGens PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${SC_SOURCE_DIR}/include/plugin_interface"
  "${SC_SOURCE_DIR}/include/common" "${SC_SOURCE_DIR}/common"
  "${SC_SOURCE_DIR}/external_libraries/boost"
  "${sndfile_SOURCE_DIR}/include" "${sndfile_BINARY_DIR}/include")
target_compile_definitions(DiskIO_UGens PRIVATE _USE_MATH_DEFINES NOMINMAX)
if(MSVC)
  target_compile_options(DiskIO_UGens PRIVATE /utf-8)
endif()
target_link_libraries(DiskIO_UGens PRIVATE sndfile Threads::Threads)
set_target_properties(DiskIO_UGens PROPERTIES PREFIX "" SUFFIX ".scx")
option(DEXTER_DISKIO_TESTS "Build the isolated actual-UGen lifecycle/audio harness" ON)
if(DEXTER_DISKIO_TESTS)
  add_executable(diskio-tests tests.cpp)
  target_compile_features(diskio-tests PRIVATE cxx_std_17)
  target_include_directories(diskio-tests PRIVATE
    "${SC_SOURCE_DIR}/include/plugin_interface" "${SC_SOURCE_DIR}/include/common"
    "${SC_SOURCE_DIR}/common" "${sndfile_SOURCE_DIR}/include" "${sndfile_BINARY_DIR}/include")
  target_link_libraries(diskio-tests PRIVATE sndfile Threads::Threads ${CMAKE_DL_LIBS})
  target_compile_definitions(diskio-tests PRIVATE NOMINMAX _USE_MATH_DEFINES)
  if(MSVC)
    target_compile_options(diskio-tests PRIVATE /utf-8)
  endif()
  if(WIN32)
    add_custom_command(TARGET DiskIO_UGens POST_BUILD COMMAND "${CMAKE_COMMAND}" -E
      copy_if_different "$<TARGET_FILE:sndfile>" "$<TARGET_FILE_DIR:DiskIO_UGens>")
    add_custom_command(TARGET diskio-tests POST_BUILD COMMAND "${CMAKE_COMMAND}" -E
      copy_if_different "$<TARGET_FILE:sndfile>" "$<TARGET_FILE_DIR:diskio-tests>")
  endif()
endif()
