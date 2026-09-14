find_package(LLVM CONFIG QUIET)
if(NOT LLVM_FOUND)
  message(FATAL_ERROR
    "NanoPass requires an LLVM development installation discoverable by CMake.\n"
    "Set LLVM_DIR to the directory containing LLVMConfig.cmake, for example:\n"
    "  cmake -S . -B build -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm")
endif()

if(LLVM_PACKAGE_VERSION VERSION_LESS "14.0")
  message(FATAL_ERROR
    "NanoPass requires LLVM 14 or newer; found ${LLVM_PACKAGE_VERSION}.")
endif()

message(STATUS "NanoPass using LLVM ${LLVM_PACKAGE_VERSION}")

# ── Main compiler executable ────────────────────────────────────────────────
add_executable(nano
    main.cpp
    codegen/llvm_codegen.cpp
    codegen/lower_stmt.cpp
    codegen/lower_expr.cpp
)

target_compile_features(nano PRIVATE cxx_std_20)

target_include_directories(nano PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LLVM_INCLUDE_DIRS}
)

# LLVM_DEFINITIONS may contain -D flags; pass them as compiler options.
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")
target_compile_options(nano PRIVATE ${LLVM_DEFINITIONS_LIST})

llvm_map_components_to_libnames(LLVM_LIBS core support)
target_link_libraries(nano PRIVATE ${LLVM_LIBS})

# ── Sanitizers (optional) ────────────────────────────────────────────────────
if(NANO_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(nano PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(nano PRIVATE -fsanitize=address,undefined)
endif()

# ── Test suite ───────────────────────────────────────────────────────────────
if(NANO_BUILD_TESTS)
  # CTest must be enabled at (or before) the directory that registers tests.
  # We call enable_testing() here so it works whether or not the parent
  # CMakeLists.txt also calls it.
  enable_testing()

  # ── Frontend smoke test ───────────────────────────────────────────────────
  add_executable(nano_frontend_tests
      ${CMAKE_SOURCE_DIR}/tests/frontend_smoke.cpp
  )
  target_compile_features(nano_frontend_tests PRIVATE cxx_std_20)
  target_include_directories(nano_frontend_tests PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  # ── CTest registrations ───────────────────────────────────────────────────
  # nano_help: verify the CLI exits 0 when called with --help
  add_test(
      NAME nano_help
      COMMAND nano --help
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

  # nano_frontend_smoke: forward calls, unterminated comments,
  # source-location diagnostics
  add_test(
      NAME nano_frontend_smoke
      COMMAND nano_frontend_tests
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )

  # ── Per-example integration tests ────────────────────────────────────────
  # Each test emits LLVM IR from an example program. A non-zero exit means
  # the frontend or LLVM verifier rejected the program — a real regression.
  foreach(example hello factorial fibonacci fizzbuzz loops branching functions desugaring)
    add_test(
        NAME emit_${example}
        COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/${example}.nano --format=llvm
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    # desugar-format test (no LLVM needed, pure AST pass)
    add_test(
        NAME desugar_${example}
        COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/${example}.nano --format=desugar
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    # verify-command test
    add_test(
        NAME verify_${example}
        COMMAND nano verify ${CMAKE_SOURCE_DIR}/examples/${example}.nano
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  endforeach()
endif()
