# Альтернатива build.sh: сборка прохода через CMake.
#   cmake -S . -B build -DLT_LLVM_INSTALL_DIR=$(llvm-config --prefix)
#   cmake --build build
# Результат: build/libTraceUsePass.so
cmake_minimum_required(VERSION 3.13)
project(TraceUsePass LANGUAGES CXX)

if(NOT DEFINED LT_LLVM_INSTALL_DIR)
  execute_process(COMMAND llvm-config --prefix OUTPUT_VARIABLE LT_LLVM_INSTALL_DIR
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
list(APPEND CMAKE_PREFIX_PATH "${LT_LLVM_INSTALL_DIR}/lib/cmake/llvm")

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Using LLVM ${LLVM_PACKAGE_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(TraceUsePass MODULE TraceUsePass.cpp)
target_include_directories(TraceUsePass PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(TraceUsePass PRIVATE ${LLVM_DEFINITIONS})
set_target_properties(TraceUsePass PROPERTIES PREFIX "lib" OUTPUT_NAME "TraceUsePass")

# На macOS символы LLVM разрешаются при загрузке в opt/clang.
if(APPLE)
  target_link_options(TraceUsePass PRIVATE -undefined dynamic_lookup)
endif()
