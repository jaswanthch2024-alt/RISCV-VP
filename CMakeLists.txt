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
option(BUILD_ROBUST_HEX "Build robust_system_test hex images" ON)

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

# Core sources = all minus the two mains (robust filtering by filename)
set(SRC_CORE ${SRC_ALL})
list(FILTER SRC_CORE EXCLUDE REGEX ".*/(Simulator|VPMain)\\.cpp$")
# Also exclude M_extension.cpp explicit specializations to avoid ODR with header implementations
list(FILTER SRC_CORE EXCLUDE REGEX ".*/M_extension\\.cpp$")
# Exclude 7-stage pipeline files (only use 2-stage)
list(FILTER SRC_CORE EXCLUDE REGEX ".*/CPU_P32\\.cpp$")
list(FILTER SRC_CORE EXCLUDE REGEX ".*/CPU_P64\\.cpp$")

# Core library with all SystemC/TLM modules
add_library(riscv_tlm_core ${SRC_CORE})
set_property(TARGET riscv_tlm_core PROPERTY POSITION_INDEPENDENT_CODE ON)
if(TARGET spdlog::spdlog)
  target_link_libraries(riscv_tlm_core PUBLIC SystemC::systemc spdlog::spdlog)
else()
  target_link_libraries(riscv_tlm_core PUBLIC SystemC::systemc spdlog::spdlog_header_only)
endif()

# Target-specific warnings and optimizations
if(MSVC)
  target_compile_options(riscv_tlm_core PRIVATE /W3 /EHsc /permissive-)
  if(ENABLE_STRICT)
    target_compile_options(riscv_tlm_core PRIVATE /WX)
  endif()
  # Do not force /O2 here to avoid conflict with Debug runtime flags (/RTC1). Use generator-set configs.
else()
  target_compile_options(riscv_tlm_core PRIVATE -O3 -g -Wall -Wextra -Wunused-function -Wpedantic)
  if(ENABLE_STRICT)
    target_compile_options(riscv_tlm_core PRIVATE -Werror)
  endif()
endif()

# Allow deprecated IEEE API usages (SC_HAS_PROCESS etc.)
target_compile_definitions(riscv_tlm_core PRIVATE SC_ALLOW_DEPRECATED_IEEE_API)

# Ensure public headers are visible to dependents
target_include_directories(riscv_tlm_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc ${SystemC_INCLUDE_DIRS})

# If pipelined ISS is enabled, add 2-stage pipeline sources (for VP only)
if(ENABLE_PIPELINED_ISS)
  target_sources(riscv_tlm_core PRIVATE 
    src/CPU_P32_2.cpp 
    src/CPU_P64_2.cpp
  )
  target_compile_definitions(riscv_tlm_core PUBLIC ENABLE_PIPELINED_ISS=1)
  target_include_directories(riscv_tlm_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)
endif()

# Always add simple non-pipelined CPU (for TLM simulator)
target_sources(riscv_tlm_core PRIVATE 
  src/CPU_Simple.cpp
)

# Legacy simulator executable (uses non-pipelined LT CPU)
add_executable(RISCV_TLM ${SRC_SIMULATOR})
target_link_libraries(RISCV_TLM PRIVATE riscv_tlm_core)
# No extra /O2 on MSVC to avoid conflicting with build type
if(NOT MSVC)
  target_compile_options(RISCV_TLM PRIVATE -O3)
endif()

# Virtual Prototype executable
add_executable(RISCV_VP ${SRC_VP_MAIN} src/VPTop.cpp)
target_link_libraries(RISCV_VP PRIVATE riscv_tlm_core)
if(NOT MSVC)
  target_compile_options(RISCV_VP PRIVATE -O3)
endif()

# Robust test hex targets (RV32/RV64) using external toolchains if available
if(BUILD_ROBUST_HEX)
  find_program(RISCV32_GCC riscv32-unknown-elf-gcc)
  find_program(RISCV32_OBJCOPY riscv32-unknown-elf-objcopy)
  find_program(RISCV64_GCC riscv64-unknown-elf-gcc)
  find_program(RISCV64_OBJCOPY riscv64-unknown-elf-objcopy)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/hex)
  if(RISCV64_GCC AND RISCV64_OBJCOPY)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/tests/hex/robust_system_test64.hex
      COMMAND ${RISCV64_GCC} -march=rv64imac -mabi=lp64 -O2 -nostdlib -Wl,--entry=main -Wl,--gc-sections
              ${CMAKE_CURRENT_SOURCE_DIR}/tests/full_system/robust_system_test.c -o robust_rv64.elf
      COMMAND ${RISCV64_OBJCOPY} -O ihex robust_rv64.elf ${CMAKE_CURRENT_SOURCE_DIR}/tests/hex/robust_system_test64.hex
      BYPRODUCTS robust_rv64.elf
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Building RV64 robust_system_test hex"
      VERBATIM)
    add_custom_target(robust_rv64_hex DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/hex/robust_system_test64.hex)
  else()
    message(STATUS "RV64 toolchain not found; robust_system_test64.hex target disabled")
  endif()
  if(RISCV32_GCC AND RISCV32_OBJCOPY)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/tests/hex/robust_system_test.hex
      COMMAND ${RISCV32_GCC} -march=rv32imac -mabi=ilp32 -O2 -nostdlib -Wl,--entry=main -Wl,--gc-sections
              ${CMAKE_CURRENT_SOURCE_DIR}/tests/full_system/robust_system_test.c -o robust_rv32.elf
      COMMAND ${RISCV32_OBJCOPY} -O ihex robust_rv32.elf ${CMAKE_CURRENT_SOURCE_DIR}/tests/hex/robust_system_test.hex
      BYPRODUCTS robust_rv32.elf
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Building RV32 robust_system_test hex"
      VERBATIM)
    add_custom_target(robust_rv32_hex DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/hex/robust_system_test.hex)
  endif()
endif()

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

