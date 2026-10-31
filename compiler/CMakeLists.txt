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

# ── Compile options shared by all targets ────────────────────────────────────
# Pass manager and AST headers are header-only (no .cpp files).
# All targets that include compiler/ headers need the compiler/ root on their
# include path.

# ── Main compiler executable ────────────────────────────────────────────────
# AOT executables link this compact C ABI runtime, while the JIT resolves the
# same symbols from the compiler process. Keep it free of LLVM dependencies.
add_library(nano_runtime STATIC backend/llvm/llvm_runtime.cpp)
target_compile_features(nano_runtime PRIVATE cxx_std_20)
target_include_directories(nano_runtime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set(NANO_SOURCES
    main.cpp
    codegen/llvm_codegen.cpp
    codegen/lower_stmt.cpp
    codegen/lower_expr.cpp
    backend/llvm/ssa_llvm_lowering.cpp
    backend/llvm/llvm_runtime.cpp
)
if(NANO_BUILD_JIT)
  list(APPEND NANO_SOURCES backend/llvm/ssa_llvm_jit.cpp)
endif()
add_executable(nano ${NANO_SOURCES})
add_dependencies(nano nano_runtime)

target_compile_features(nano PRIVATE cxx_std_20)

target_include_directories(nano PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LLVM_INCLUDE_DIRS}
)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")
target_compile_options(nano PRIVATE ${LLVM_DEFINITIONS_LIST})

set(NANO_LLVM_COMPONENTS core support)
if(NANO_BUILD_JIT)
  list(APPEND NANO_LLVM_COMPONENTS orcjit native)
  target_compile_definitions(nano PRIVATE NANO_HAS_ORC_JIT=1)
endif()
llvm_map_components_to_libnames(LLVM_LIBS ${NANO_LLVM_COMPONENTS})
target_link_libraries(nano PRIVATE ${LLVM_LIBS})
target_compile_definitions(nano PRIVATE
    NANO_RUNTIME_LIBRARY_PATH="$<TARGET_FILE:nano_runtime>"
    NANO_HOST_CXX="${CMAKE_CXX_COMPILER}")

if(UNIX AND NANO_BUILD_JIT)
  target_link_options(nano PRIVATE -rdynamic)
endif()

# Reproducible native differential campaign.  It is intentionally a separate
# executable: Core/SSA correctness remains available without LLVM, while this
# target exists only when the optional ORC runtime is enabled.
if(NANO_BUILD_JIT)
  add_executable(nano_jit_correctness
      ${CMAKE_SOURCE_DIR}/tools/nano-jit-correctness.cpp
      backend/llvm/ssa_llvm_lowering.cpp
      backend/llvm/ssa_llvm_jit.cpp
      backend/llvm/llvm_runtime.cpp)
  target_compile_features(nano_jit_correctness PRIVATE cxx_std_20)
  target_include_directories(nano_jit_correctness PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR} ${LLVM_INCLUDE_DIRS})
  target_compile_options(nano_jit_correctness PRIVATE ${LLVM_DEFINITIONS_LIST})
  target_link_libraries(nano_jit_correctness PRIVATE ${LLVM_LIBS})
  if(UNIX)
    target_link_options(nano_jit_correctness PRIVATE -rdynamic)
  endif()
  if(NANO_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(nano_jit_correctness PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(nano_jit_correctness PRIVATE -fsanitize=address,undefined)
  endif()
endif()

# Enable useful warnings for NanoPass's own code.
# We avoid -Werror globally because LLVM headers can trigger warnings we can't
# fix; the PRIVATE scope restricts these flags to our own compilation units.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(nano PRIVATE
    -Wall -Wextra -Wno-unused-parameter
  )
endif()

# ── Sanitizers (optional) ────────────────────────────────────────────────────
if(NANO_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(nano PRIVATE
    -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(nano PRIVATE
    -fsanitize=address,undefined)
endif()

# ── Test suite ───────────────────────────────────────────────────────────────
if(NANO_BUILD_TESTS)
  enable_testing()

  # ── Frontend smoke test ─────────────────────────────────────────────────
  add_executable(nano_frontend_tests
      ${CMAKE_SOURCE_DIR}/tests/frontend_smoke.cpp
  )
  target_compile_features(nano_frontend_tests PRIVATE cxx_std_20)
  target_include_directories(nano_frontend_tests PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
  )
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(nano_frontend_tests PRIVATE
      -Wall -Wextra -Wno-unused-parameter)
  endif()

# LLVM-independent Phase 7 execution tool.  It is intentionally separate from
# `nano` so semantic testing remains usable when an LLVM package is absent.
add_executable(nano_correctness ${CMAKE_SOURCE_DIR}/tools/nano-correctness.cpp)
target_compile_features(nano_correctness PRIVATE cxx_std_20)
target_include_directories(nano_correctness PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(nano_correctness PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
  if(NANO_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(nano_frontend_tests PRIVATE
      -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(nano_frontend_tests PRIVATE
      -fsanitize=address,undefined)
  endif()

  # ── Pass manager unit tests ─────────────────────────────────────────────
  # This target does NOT link LLVM — the pass infrastructure and AST are
  # header-only; no LLVM types appear in the test file.
  add_executable(nano_pass_manager_tests
      ${CMAKE_SOURCE_DIR}/tests/pass_manager_tests.cpp
  )
  target_compile_features(nano_pass_manager_tests PRIVATE cxx_std_20)
  target_include_directories(nano_pass_manager_tests PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
  )
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(nano_pass_manager_tests PRIVATE
      -Wall -Wextra -Wno-unused-parameter)
  endif()
  if(NANO_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(nano_pass_manager_tests PRIVATE
      -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(nano_pass_manager_tests PRIVATE
      -fsanitize=address,undefined)
  endif()

  # ── IR pipeline unit tests ─────────────────────────────────────────────
  # Also does not link LLVM — the IR pipeline headers are header-only;
  # no LLVM types appear in ir_tests.cpp.
  add_executable(nano_ir_tests
      ${CMAKE_SOURCE_DIR}/tests/ir_tests.cpp
  )
  target_compile_features(nano_ir_tests PRIVATE cxx_std_20)
  target_include_directories(nano_ir_tests PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
  )
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(nano_ir_tests PRIVATE
      -Wall -Wextra -Wno-unused-parameter)
    # Header-only SSA analyses make the Windows/MinGW test translation unit
    # exceed COFF's default section count; this affects only that toolchain.
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(nano_ir_tests PRIVATE -Wa,-mbig-obj)
    endif()
  endif()

  if(NANO_BUILD_JIT)
  add_executable(nano_llvm_backend_tests
      ${CMAKE_SOURCE_DIR}/tests/llvm_backend_tests.cpp
      backend/llvm/ssa_llvm_lowering.cpp
      backend/llvm/ssa_llvm_jit.cpp
      backend/llvm/llvm_runtime.cpp)
  target_compile_features(nano_llvm_backend_tests PRIVATE cxx_std_20)
  target_include_directories(nano_llvm_backend_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${LLVM_INCLUDE_DIRS})
  target_compile_options(nano_llvm_backend_tests PRIVATE ${LLVM_DEFINITIONS_LIST})
  target_link_libraries(nano_llvm_backend_tests PRIVATE ${LLVM_LIBS})
  if(UNIX)
    target_link_options(nano_llvm_backend_tests PRIVATE -rdynamic)
  endif()
  if(NANO_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(nano_llvm_backend_tests PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(nano_llvm_backend_tests PRIVATE -fsanitize=address,undefined)
  endif()
  endif()
  if(NANO_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(nano_ir_tests PRIVATE
      -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(nano_ir_tests PRIVATE
      -fsanitize=address,undefined)
  endif()

  # ── CTest registrations ────────────────────────────────────────────────
  add_test(
      NAME nano_help
      COMMAND nano --help
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  add_test(
      NAME nano_version
      COMMAND nano --version
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  add_test(
      NAME nano_passes_command
      COMMAND nano passes
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  add_test(
      NAME nano_analyses_command
      COMMAND nano analyses
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  add_test(
      NAME nano_frontend_smoke
      COMMAND nano_frontend_tests
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  add_test(
      NAME nano_pass_manager_unit
      COMMAND nano_pass_manager_tests
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  add_test(
      NAME nano_ir_unit
      COMMAND nano_ir_tests
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  if(NANO_BUILD_JIT)
    add_test(NAME nano_llvm_backend_unit COMMAND nano_llvm_backend_tests
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME nano_ssa_backend_emit
             COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/factorial.nano
                      --format=llvm --backend=ssa -O3
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME nano_ssa_backend_jit
             COMMAND nano jit ${CMAKE_SOURCE_DIR}/examples/factorial.nano -O3
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME nano_ssa_backend_object
             COMMAND nano compile ${CMAKE_SOURCE_DIR}/examples/factorial.nano
                      --backend=ssa --emit-obj -o ${CMAKE_CURRENT_BINARY_DIR}/nano_ssa_factorial.o
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set(NANO_SSA_AOT_EXE ${CMAKE_CURRENT_BINARY_DIR}/nano_ssa_factorial${CMAKE_EXECUTABLE_SUFFIX})
    add_test(NAME nano_ssa_backend_executable
             COMMAND nano compile ${CMAKE_SOURCE_DIR}/examples/factorial.nano
                      --backend=ssa --emit-exe -o ${NANO_SSA_AOT_EXE}
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME nano_ssa_backend_executable_run COMMAND ${NANO_SSA_AOT_EXE})
    set_tests_properties(nano_ssa_backend_executable_run PROPERTIES
        DEPENDS nano_ssa_backend_executable)
  endif()

  # ── Per-example integration tests ────────────────────────────────────
  foreach(example
      hello factorial fibonacci fizzbuzz loops
      branching functions desugaring)

    add_test(
        NAME emit_${example}
        COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/${example}.nano
                         --format=llvm
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    add_test(
        NAME desugar_${example}
        COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/${example}.nano
                         --format=desugar
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    add_test(
        NAME verify_${example}
        COMMAND nano verify ${CMAKE_SOURCE_DIR}/examples/${example}.nano
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  endforeach()

  # ── Pass-manager smoke tests via CLI ──────────────────────────────────
  # --debug-pass-manager must mention each pass name exactly once
  add_test(
      NAME debug_pass_manager_smoke
      COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/loops.nano
                       --format=desugar
                       --debug-pass-manager
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  # --print-pipeline must exit 0
  add_test(
      NAME print_pipeline_smoke
      COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/hello.nano
                       --format=desugar
                       --print-pipeline
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  # --stop-after must exit 0
  add_test(
      NAME stop_after_smoke
      COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/desugaring.nano
                       --format=desugar
                       --stop-after=desugar-for
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  # --disable-pass must exit 0
  add_test(
      NAME disable_pass_smoke
      COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/hello.nano
                       --format=desugar
                       --disable-pass=desugar-inc-dec
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  # Unknown pass ID in --stop-after must exit non-zero
  add_test(
      NAME stop_after_invalid_pass
      COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/hello.nano
                       --format=desugar
                       --stop-after=this-pass-does-not-exist
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  set_tests_properties(stop_after_invalid_pass PROPERTIES WILL_FAIL TRUE)

  # ── Phase 3 IR format tests ────────────────────────────────────────────
  foreach(example factorial loops branching functions)
    add_test(
        NAME core_${example}
        COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/${example}.nano
                         --format=core
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    add_test(
        NAME anf_${example}
        COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/${example}.nano
                         --format=anf
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    add_test(
        NAME cfg_${example}
        COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/${example}.nano
                         --format=cfg
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    add_test(
        NAME ssa_${example}
        COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/${example}.nano
                         --format=ssa
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    add_test(
        NAME graph_cfg_${example}
        COMMAND nano graph ${CMAKE_SOURCE_DIR}/examples/${example}.nano
                         --cfg
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    add_test(
        NAME graph_dominator_${example}
        COMMAND nano graph ${CMAKE_SOURCE_DIR}/examples/${example}.nano
                         --dominator-tree
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    add_test(
        NAME explain_${example}
        COMMAND nano explain ${CMAKE_SOURCE_DIR}/examples/${example}.nano
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  endforeach()

  foreach(example optimizer sccp dce cse)
    add_test(
        NAME optimized_${example}
        COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/${example}.nano --format=optimized
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  endforeach()

  add_test(
      NAME optimize_custom_pipeline
      COMMAND nano optimize ${CMAKE_SOURCE_DIR}/examples/optimizer.nano
                       --passes=constant-fold,dce
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  add_test(
      NAME optimize_unknown_pass
      COMMAND nano optimize ${CMAKE_SOURCE_DIR}/examples/optimizer.nano
                       --passes=not-a-real-ssa-pass
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  set_tests_properties(optimize_unknown_pass PROPERTIES WILL_FAIL TRUE)

  # short_circuit.nano — correct && / || lowering in ANF/CFG
  foreach(fmt core anf cfg)
    add_test(
        NAME ${fmt}_short_circuit
        COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/short_circuit.nano
                         --format=${fmt}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  endforeach()

  # control_flow.nano — break/continue parsing and CFG lowering
  foreach(fmt core cfg)
    add_test(
        NAME ${fmt}_control_flow
        COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/control_flow.nano
                         --format=${fmt}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  endforeach()

  # errors.nano — semantic error must exit non-zero and not crash
  add_test(
      NAME errors_reject
      COMMAND nano emit ${CMAKE_SOURCE_DIR}/examples/errors.nano
                       --format=core
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  )
  set_tests_properties(errors_reject PROPERTIES WILL_FAIL TRUE)

endif() # NANO_BUILD_TESTS
