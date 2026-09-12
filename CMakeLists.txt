cmake_minimum_required(VERSION 3.22.1)
project(ggfm_patch LANGUAGES C CXX)

find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT ANDROID)
  message(FATAL_ERROR "The production patch must be cross-compiled with the Android NDK")
endif()
if(NOT DEFINED GGFM_DOBBY_LIBRARY OR NOT EXISTS "${GGFM_DOBBY_LIBRARY}")
  message(FATAL_ERROR "GGFM_DOBBY_LIBRARY must point to the pinned Dobby artifact for the selected ABI")
endif()
string(LENGTH "${GGFM_DOBBY_SHA256}" GGFM_DOBBY_SHA256_LENGTH)
if(NOT DEFINED GGFM_DOBBY_SHA256 OR NOT GGFM_DOBBY_SHA256_LENGTH EQUAL 64 OR GGFM_DOBBY_SHA256 MATCHES "[^0-9A-Fa-f]")
  message(FATAL_ERROR "GGFM_DOBBY_SHA256 must pin the Dobby release artifact")
endif()
if(NOT DEFINED GGFM_SERVER_LIBRARY OR NOT EXISTS "${GGFM_SERVER_LIBRARY}")
  message(FATAL_ERROR "GGFM_SERVER_LIBRARY must point to the pinned Rust server artifact")
endif()
string(LENGTH "${GGFM_SERVER_SHA256}" GGFM_SERVER_SHA256_LENGTH)
if(NOT DEFINED GGFM_SERVER_SHA256 OR NOT GGFM_SERVER_SHA256_LENGTH EQUAL 64 OR GGFM_SERVER_SHA256 MATCHES "[^0-9A-Fa-f]")
  message(FATAL_ERROR "GGFM_SERVER_SHA256 must pin the Rust server release artifact")
endif()
file(SHA256 "${GGFM_DOBBY_LIBRARY}" GGFM_DOBBY_ACTUAL_SHA256)
file(SHA256 "${GGFM_SERVER_LIBRARY}" GGFM_SERVER_ACTUAL_SHA256)
string(TOLOWER "${GGFM_DOBBY_SHA256}" GGFM_DOBBY_SHA256)
string(TOLOWER "${GGFM_SERVER_SHA256}" GGFM_SERVER_SHA256)
if(NOT GGFM_DOBBY_ACTUAL_SHA256 STREQUAL GGFM_DOBBY_SHA256)
  message(FATAL_ERROR "Dobby artifact SHA-256 mismatch")
endif()
if(NOT GGFM_SERVER_ACTUAL_SHA256 STREQUAL GGFM_SERVER_SHA256)
  message(FATAL_ERROR "Rust server artifact SHA-256 mismatch")
endif()
set(GGFM_POLICY_MANIFEST
    "${CMAKE_CURRENT_SOURCE_DIR}/policy/memorial-policy.v1.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${GGFM_POLICY_MANIFEST}")
file(SHA256 "${GGFM_POLICY_MANIFEST}" GGFM_POLICY_SHA256)
string(TOUPPER "${GGFM_POLICY_SHA256}" GGFM_POLICY_SHA256)

add_library(dobby SHARED IMPORTED)
set_target_properties(dobby PROPERTIES IMPORTED_LOCATION "${GGFM_DOBBY_LIBRARY}")
add_library(ggfm_server SHARED IMPORTED)
set_target_properties(ggfm_server PROPERTIES IMPORTED_LOCATION "${GGFM_SERVER_LIBRARY}")

set(GGFM_COMPATIBILITY_MANIFEST
    "${CMAKE_CURRENT_SOURCE_DIR}/compatibility/8.0.0.json"
    CACHE FILEPATH "Verified compatibility manifest for this exact target ABI")
file(READ "${GGFM_COMPATIBILITY_MANIFEST}" GGFM_COMPATIBILITY_JSON)
string(JSON GGFM_COMPATIBILITY_ABI GET "${GGFM_COMPATIBILITY_JSON}" source abi)
if(NOT GGFM_COMPATIBILITY_ABI STREQUAL ANDROID_ABI)
  message(FATAL_ERROR "Compatibility manifest ABI does not match ANDROID_ABI")
endif()
set(GGFM_GENERATED_INCLUDE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ggfm/generated_hook_targets.inc")
set(GGFM_GENERATED_LOCALIZATION
    "${CMAKE_CURRENT_BINARY_DIR}/generated/ggfm/generated_localization.inc")
add_custom_command(
  OUTPUT "${GGFM_GENERATED_INCLUDE}"
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_hook_targets.py"
          "${GGFM_COMPATIBILITY_MANIFEST}" "${GGFM_GENERATED_INCLUDE}"
  DEPENDS "${GGFM_COMPATIBILITY_MANIFEST}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_hook_targets.py"
  VERBATIM
)
add_custom_target(ggfm_generated_hooks DEPENDS "${GGFM_GENERATED_INCLUDE}")
add_custom_command(
  OUTPUT "${GGFM_GENERATED_LOCALIZATION}"
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_localization.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/localization/strings.json"
          "${GGFM_GENERATED_LOCALIZATION}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/localization/strings.json"
          "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_localization.py"
  VERBATIM
)
add_custom_target(ggfm_generated_localization DEPENDS "${GGFM_GENERATED_LOCALIZATION}")

add_library(ggfm_patch_core STATIC
  "${GGFM_GENERATED_INCLUDE}"
  "${GGFM_GENERATED_LOCALIZATION}"
  native-core/src/core.cpp
  native-core/src/admin_bridge.cpp
  native-core/src/player_prefs.cpp
  native-core/src/request_context.cpp
  native-core/src/memorial_ui.cpp
  native-core/src/gameplay_compat.cpp
  native-core/src/client_save.cpp
  native-core/src/follower_row_cache.cpp
  native-core/src/fan_multiplier_cache.cpp
  native-core/src/runtime_hooks.cpp
)
add_dependencies(ggfm_patch_core ggfm_generated_hooks)
add_dependencies(ggfm_patch_core ggfm_generated_localization)
target_include_directories(ggfm_patch_core PUBLIC native-core/include
                           PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_options(ggfm_patch_core PRIVATE -Wall -Wextra -Werror -fvisibility=hidden)

add_executable(ggfm_compat_tests EXCLUDE_FROM_ALL native-core/tests/gameplay_rules_test.cpp)
target_include_directories(ggfm_compat_tests PRIVATE native-core/include)
target_compile_options(ggfm_compat_tests PRIVATE -Wall -Wextra -Werror)

add_library(ggfm_bootstrap SHARED
  native-core/src/dobby_backend.cpp
  native-core/src/standalone_loader.cpp
  native-core/src/bootstrap_jni.cpp
)
target_link_libraries(ggfm_bootstrap PRIVATE ggfm_patch_core dobby ggfm_server android log)
target_compile_definitions(ggfm_bootstrap PRIVATE
  GGFM_POLICY_SHA256="${GGFM_POLICY_SHA256}")

option(GGFM_BUILD_LSPOSED_ADAPTER "Build the development-only LSPosed adapter" OFF)
if(GGFM_BUILD_LSPOSED_ADAPTER)
  add_library(ggfm_lsposed_adapter SHARED native-core/src/lsposed_adapter.cpp)
  target_link_libraries(ggfm_lsposed_adapter PRIVATE ggfm_patch_core log)
endif()
