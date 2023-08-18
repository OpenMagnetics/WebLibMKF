cmake_minimum_required(VERSION 3.5.1)
project (libMKF)
option(JS_ONLY "Compiles to native JS (No WASM)" OFF)

add_definitions(-std=c++23 -O3)
include_directories(include)

SET(CMAKE_BUILD_TYPE RelWithDebInfo)

SET(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-deprecated-declarations -Wno-unused-parameter -Wno-switch")

option(BUILD_TESTS      "Build tests"    OFF)
option(BUILD_EXAMPLES   "Build examples" OFF)
option(UTPP_INCLUDE_TESTS_IN_BUILD   "Build tests" OFF)

include(FetchContent)
include(ExternalProject)

FetchContent_Declare(MKF
        GIT_REPOSITORY https://github.com/OpenMagnetics/MKF.git
        GIT_TAG get-rid-of-boost-dependency)


FetchContent_Declare(
       mas
       GIT_REPOSITORY https://github.com/OpenMagnetics/MAS.git
       GIT_TAG main
)
FetchContent_GetProperties(mas)
message(STATUS ${mas_POPULATED})
if(NOT mas_POPULATED)
  FetchContent_Populate(mas)
endif()
message(STATUS ${mas_SOURCE_DIR})

add_subdirectory(${mas_SOURCE_DIR} EXCLUDE_FROM_ALL)

SET(MAS_DIRECTORY "${CMAKE_BINARY_DIR}/MAS/")
SET(MAS_DIR ${mas_SOURCE_DIR})
option(INCLUDE_MKF_TESTS  "Build tests"    OFF)
option(INCLUDE_PYMKF  "Build PyMKF tests"    OFF)
message(STATUS ${MAS_DIRECTORY})


FetchContent_MakeAvailable(MKF)
SET(CMAKE_CXX_FLAGS "-Wall -Wextra")

find_program(NAMES quicktype REQUIRED)
find_program(NAMES libgtest-dev REQUIRED)


file(GLOB SOURCES src/*.cpp)
add_executable(libMKF ${SOURCES})

include_directories("${CMAKE_BINARY_DIR}/_deps/json-src/include/nlohmann/")
include_directories("${CMAKE_BINARY_DIR}/_deps/pybind11-src/include/")
include_directories("${CMAKE_BINARY_DIR}/_deps/pybind11_json-src/include/pybind11_json/")
include_directories("${CMAKE_BINARY_DIR}/_deps/unittest++-src/UnitTest++/")
include_directories("${CMAKE_BINARY_DIR}/_deps/json-src/include/nlohmann/")
include_directories("${CMAKE_BINARY_DIR}/_deps/magic-enum-src/include")
include_directories("${CMAKE_BINARY_DIR}/_deps/svg-src/src")
include_directories("${CMAKE_BINARY_DIR}/_deps/spline-src/src")
include_directories("${CMAKE_BINARY_DIR}/_deps/json-src/include/")
include_directories("${MAS_DIRECTORY}")
message(STATUS MAS_DIRECTORY)
message(STATUS ${MAS_DIRECTORY})

target_link_libraries(libMKF MKF)


if(JS_ONLY)
    message(STATUS "Setting compilation target to native JavaScript")
    set(CMAKE_EXECUTABLE_SUFFIX ".js")
    set_target_properties(libMKF PROPERTIES LINK_FLAGS "-s WASM=0 -s EXPORTED_FUNCTIONS='[_main, _getCoreShapeNames, _process_operating_point]' -s EXPORT_ES6=1")
else(JS_ONLY)
    message(STATUS "Setting compilation target to WASM")
    set(CMAKE_EXECUTABLE_SUFFIX ".wasm.js")
    set_target_properties(libMKF PROPERTIES LINK_FLAGS "-s WASM=1  -s EXPORT_ES6=1 -l embind --bind")
endif(JS_ONLY)