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

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_STRICT "Treat warnings as errors" OFF)
option(BUILD_DOC "Build documentation" ON)
# Make tests opt-in (clean project)
option(BUILD_TESTING "Build tests" OFF)
option(ENABLE_PIPELINED_ISS "Enable pipelined ISS" ON)

# Portable warning/optimization flags for GCC/Clang
add_compile_options(-O3 -g -Wall -Wextra -Wunused-function -Wpedantic)
if(ENABLE_STRICT)
  add_compile_options(-Werror)
endif()

include_directories(./inc/)

find_package(SystemC REQUIRED)
message(STATUS "SYSTEMC_FOUND: ${SYSTEMC_FOUND}")
message(STATUS "SystemC include dirs: ${SYSTEMC_INCLUDE_DIRS}")
message(STATUS "SystemC libraries: ${SYSTEMC_LIBRARIES}")

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

# Ensure public headers are visible to dependents
target_include_directories(riscv_tlm_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

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

# Clean project: tests disabled by default and no CTest targets

