cmake_minimum_required(VERSION 3.5.1)
project (libMKF)
option(JS_ONLY "Compiles to native JS (No WASM)" OFF)

add_definitions(-std=c++23)
include_directories(include)

# SET(CMAKE_BUILD_TYPE MinSizeRel)
SET(CMAKE_BUILD_TYPE Release)
# SET(CMAKE_BUILD_TYPE RelWithDebInfo)

SET(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-deprecated-declarations -Wno-unused-parameter -Wno-switch -fwasm-exceptions")

option(BUILD_TESTS      "Build tests"    OFF)
option(BUILD_EXAMPLES   "Build examples" OFF)
option(UTPP_INCLUDE_TESTS_IN_BUILD   "Build tests" OFF)
option(INCLUDE_MKF_TESTS      "Build tests"    OFF)
option(DEBUG_WASM      "Build in debug mode"    OFF)

include(FetchContent)
include(ExternalProject)

FetchContent_Declare(MKF
        GIT_REPOSITORY https://github.com/OpenMagnetics/MKF.git
        GIT_TAG main)

FetchContent_Declare(magic-enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum
        GIT_TAG  tags/v0.9.6)

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
message(STATUS ${MAS_SOURCE_DIR})

# add_subdirectory(${MAS_SOURCE_DIR} EXCLUDE_FROM_ALL)

SET(MAS_DIRECTORY "${CMAKE_BINARY_DIR}/MAS/")
SET(MAS_DIR ${MAS_SOURCE_DIR})
message(STATUS ${MAS_DIRECTORY})


FetchContent_MakeAvailable(MKF magic-enum)
include_directories("${CMAKE_BINARY_DIR}/_deps/magic-enum-src/include/magic_enum")
# SET(CMAKE_CXX_FLAGS "-Wall -Wextra -fwasm-exceptions")

find_program(NAMES quicktype REQUIRED)
find_program(NAMES libgtest-dev REQUIRED)


add_executable(libMKF src/libMKF.cpp)
add_executable(libInsulationCoordinator src/libInsulationCoordinator.cpp)
add_executable(libAdvisers src/libAdvisers.cpp)
add_executable(libCrossReferencers src/libCrossReferencers.cpp)

include_directories("${CMAKE_BINARY_DIR}/_deps/json-src/include/nlohmann/")
include_directories("${CMAKE_BINARY_DIR}/_deps/pybind11-src/include/")
include_directories("${CMAKE_BINARY_DIR}/_deps/pybind11_json-src/include/pybind11_json/")
include_directories("${CMAKE_BINARY_DIR}/_deps/json-src/include/nlohmann/")

include_directories("${CMAKE_BINARY_DIR}/_deps/svg-src/src")
include_directories("${CMAKE_BINARY_DIR}/_deps/spline-src/src")
include_directories("${CMAKE_BINARY_DIR}/_deps/json-src/include/")
include_directories("${MAS_DIRECTORY}")
message(STATUS MAS_DIRECTORY)
message(STATUS ${MAS_DIRECTORY})

target_link_libraries(libMKF MKF)
target_link_libraries(libInsulationCoordinator MKF)
target_link_libraries(libAdvisers MKF)
target_link_libraries(libCrossReferencers MKF)


if(JS_ONLY)
    message(STATUS "Setting compilation target to native JavaScript")
    set(CMAKE_EXECUTABLE_SUFFIX ".js")
    set_target_properties(libMKF PROPERTIES LINK_FLAGS "-s WASM=0 -s EXPORTED_FUNCTIONS='[_main, _getCoreShapeNames, _process_operating_point]' -s EXPORT_ES6=1")
else(JS_ONLY)
    message(STATUS "Setting compilation target to WASM")
    set(CMAKE_EXECUTABLE_SUFFIX ".wasm.js")
    set_target_properties(libMKF PROPERTIES LINK_FLAGS "-O3 -s WASM=1 -s MAXIMUM_MEMORY=4gb -sSTACK_SIZE=67108864 -s INITIAL_MEMORY=134217728 -s ALLOW_MEMORY_GROWTH -s EXPORT_ES6=1 -std=c++23 -s EXCEPTION_CATCHING_ALLOWED=[..] -fwasm-exceptions -l embind --bind --use-preload-cache")
    set_target_properties(libInsulationCoordinator PROPERTIES LINK_FLAGS "-O3 -s WASM=1 -s MAXIMUM_MEMORY=4gb -sSTACK_SIZE=67108864 -s INITIAL_MEMORY=134217728 -s ALLOW_MEMORY_GROWTH -s EXPORT_ES6=1 -std=c++23 -s EXCEPTION_CATCHING_ALLOWED=[..] -fwasm-exceptions -l embind --bind --use-preload-cache")
    set_target_properties(libAdvisers PROPERTIES LINK_FLAGS "-O3 -s WASM=1 -s MAXIMUM_MEMORY=4gb -sSTACK_SIZE=67108864 -s INITIAL_MEMORY=134217728 -s ALLOW_MEMORY_GROWTH -s EXPORT_ES6=1 -std=c++23 -s EXCEPTION_CATCHING_ALLOWED=[..] -fwasm-exceptions -l embind --bind --use-preload-cache")
    set_target_properties(libCrossReferencers PROPERTIES LINK_FLAGS "-O3 -s WASM=1 -s MAXIMUM_MEMORY=4gb -sSTACK_SIZE=67108864 -s INITIAL_MEMORY=134217728 -s ALLOW_MEMORY_GROWTH -s EXPORT_ES6=1 -std=c++23 -s EXCEPTION_CATCHING_ALLOWED=[..] -fwasm-exceptions -l embind --bind --use-preload-cache")

    # set_target_properties(libMKF PROPERTIES LINK_FLAGS "-s WASM=1 -s MAXIMUM_MEMORY=4gb -sSTACK_SIZE=67108864 -s INITIAL_MEMORY=134217728 -s ALLOW_MEMORY_GROWTH -s EXPORT_ES6=1 -std=c++23 -g3 -s ASSERTIONS=2 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=2 -s DEMANGLE_SUPPORT=1 -s EXCEPTION_CATCHING_ALLOWED=[..] -fwasm-exceptions --source-map-base http://localhost:5173/ -l embind --bind --use-preload-cache")
    # set_target_properties(libAdvisers PROPERTIES LINK_FLAGS "-s WASM=1 -s MAXIMUM_MEMORY=4gb -sSTACK_SIZE=67108864 -s INITIAL_MEMORY=134217728 -s ALLOW_MEMORY_GROWTH -s EXPORT_ES6=1 -std=c++23 -g3 -s ASSERTIONS=2 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=2 -s DEMANGLE_SUPPORT=1 -s EXCEPTION_CATCHING_ALLOWED=[..] -fwasm-exceptions --source-map-base http://localhost:5173/ -l embind --bind --use-preload-cache")
    # set_target_properties(libCrossReferencers PROPERTIES LINK_FLAGS "-s WASM=1 -s MAXIMUM_MEMORY=4gb -sSTACK_SIZE=67108864 -s INITIAL_MEMORY=134217728 -s ALLOW_MEMORY_GROWTH -s EXPORT_ES6=1 -std=c++23 -g3 -s ASSERTIONS=2 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=2 -s DEMANGLE_SUPPORT=1 -s EXCEPTION_CATCHING_ALLOWED=[..] -fwasm-exceptions --source-map-base http://localhost:5173/ -l embind --bind --use-preload-cache")
endif(JS_ONLY)
