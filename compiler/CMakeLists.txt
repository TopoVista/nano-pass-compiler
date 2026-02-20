cmake_minimum_required(VERSION 3.20)
project(compiler LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(compiler
    main.cpp
    codegen/llvm_codegen.cpp
    codegen/lower_stmt.cpp
    codegen/lower_expr.cpp
)


target_include_directories(compiler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})



llvm_map_components_to_libnames(LLVM_LIBS core support)
target_link_libraries(compiler ${LLVM_LIBS})
