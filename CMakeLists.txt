## This project requires systemC to be compiled with cmake using C++17:
## cd <systemc directory>
## mkdir build
## cd build
## cmake ../ -DCMAKE_CXX_STANDARD=17
## make

cmake_minimum_required(VERSION 3.10)
project(RISCV_TLM)

# Prefer Ninja generator if available
if(NOT CMAKE_GENERATOR)
  find_program(NINJA_EXECUTABLE ninja)
  if(NINJA_EXECUTABLE)
    set(CMAKE_GENERATOR "Ninja" CACHE STRING "Build system generator" FORCE)
    message(STATUS "Using Ninja generator")
  endif()
endif()

# Use C++20 on MSVC to support [[likely]] attribute; otherwise default to 17
if(MSVC)
  set(CMAKE_CXX_STANDARD 20)
else()
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_STRICT "Treat warnings as errors" OFF)
option(BUILD_DOC "Build documentation" ON)
option(BUILD_TESTING "Build tests" OFF)
option(ENABLE_PIPELINED_ISS "Enable pipelined ISS" ON)
option(USE_LOCAL_SYSTEMC "Use vendored SystemC located in systemc/ subdir" ON)

# Compiler-specific warning/optimization flags (avoid passing GCC flags to MSVC)
if(MSVC)
  add_compile_options(/O2 /Zi /W3 /EHsc /permissive-)
  if(ENABLE_STRICT)
    add_compile_options(/WX)
  endif()
else()
  add_compile_options(-O3 -g -Wall -Wextra -Wunused-function -Wpedantic)
  if(ENABLE_STRICT)
    add_compile_options(-Werror)
  endif()
endif()

include_directories(./inc/)

# Try system installation first
find_package(SystemC QUIET)

if(NOT SystemC_FOUND AND USE_LOCAL_SYSTEMC)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/systemc/CMakeLists.txt")
    message(STATUS "SystemC not found via find_package. Using local systemc/ subdirectory.")
    add_subdirectory(systemc)
    # The upstream build usually defines target 'systemc'. Provide an alias matching find_package usage.
    if(TARGET systemc AND NOT TARGET SystemC::systemc)
      add_library(SystemC::systemc ALIAS systemc)
    endif()
    set(SystemC_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/systemc/src")
  else()
    message(FATAL_ERROR "SystemC not found. Clone it into systemc/ or install it so find_package(SystemC) works.")
  endif()
endif()

if(TARGET SystemC::systemc)
  message(STATUS "SystemC target available: SystemC::systemc")
else()
  message(FATAL_ERROR "Failed to obtain SystemC library target.")
endif()

message(STATUS "SystemC include dirs: ${SystemC_INCLUDE_DIRS}")

# Prefer system spdlog, fall back to bundled
find_package(spdlog CONFIG QUIET)
if(NOT spdlog_FOUND)
  message(STATUS "spdlog not found via package config. Using bundled spdlog subdirectory.")
  add_subdirectory(spdlog)
endif()

# Gather all sources
file(GLOB SRC_ALL "src/*.cpp")

# Define files that contain sc_main
set(SRC_SIMULATOR "src/Simulator.cpp")
set(SRC_VP_MAIN  "src/VPMain.cpp")

# Core sources = all minus the two mains
set(SRC_CORE ${SRC_ALL})
list(REMOVE_ITEM SRC_CORE ${SRC_SIMULATOR} ${SRC_VP_MAIN})

# Core library with all SystemC/TLM modules
add_library(riscv_tlm_core ${SRC_CORE})
set_property(TARGET riscv_tlm_core PROPERTY POSITION_INDEPENDENT_CODE ON)
if(TARGET spdlog::spdlog)
  target_link_libraries(riscv_tlm_core PUBLIC SystemC::systemc spdlog::spdlog)
else()
  target_link_libraries(riscv_tlm_core PUBLIC SystemC::systemc spdlog::spdlog_header_only)
endif()

# Target-specific warnings (avoid affecting third-party subdirs further)
if(MSVC)
  target_compile_options(riscv_tlm_core PRIVATE /W3 /wd4244 /wd4267 /wd4996)
else()
  target_compile_options(riscv_tlm_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
# Allow deprecated IEEE API usages (SC_HAS_PROCESS etc.)
target_compile_definitions(riscv_tlm_core PRIVATE SC_ALLOW_DEPRECATED_IEEE_API)

# Ensure public headers are visible to dependents
target_include_directories(riscv_tlm_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc ${SystemC_INCLUDE_DIRS})

# If pipelined ISS is enabled, add its sources and export the macro
if(ENABLE_PIPELINED_ISS)
  target_sources(riscv_tlm_core PRIVATE src/CPU_P32.cpp src/CPU_P64.cpp)
  target_compile_definitions(riscv_tlm_core PUBLIC ENABLE_PIPELINED_ISS=1)
  target_include_directories(riscv_tlm_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
endif()

# Legacy simulator executable
add_executable(RISCV_TLM ${SRC_SIMULATOR})
target_link_libraries(RISCV_TLM PRIVATE riscv_tlm_core)

# Virtual Prototype executable
add_executable(RISCV_VP ${SRC_VP_MAIN} src/VPTop.cpp)
target_link_libraries(RISCV_VP PRIVATE riscv_tlm_core)

find_package(Doxygen)
if (DOXYGEN_FOUND AND BUILD_DOC)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_SOURCE_DIR}/doc)
    message(STATUS "Doxygen build enabled")
    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_IN}
        WORKING_DIRECTORY ${DOXYGEN_OUT}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)
else()
    if(BUILD_DOC)
      message(STATUS "Doxygen not found; documentation will not be generated")
    endif()
endif()

