cmake_minimum_required(VERSION 3.21)
project(TROLLfxHeliosMirrorV2 VERSION 2.0.0 LANGUAGES CXX)

include(CTest)
find_package(Threads REQUIRED)

add_library(helios_mirror_core STATIC
    src/transform.cpp
    src/helios_output.cpp)
target_include_directories(helios_mirror_core PUBLIC include)
target_compile_features(helios_mirror_core PUBLIC cxx_std_17)
target_link_libraries(helios_mirror_core PUBLIC Threads::Threads)
set_target_properties(helios_mirror_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(helios_mirror_core PRIVATE /W4 /permissive-)
    target_compile_definitions(helios_mirror_core PRIVATE _WIN32_WINNT=0x0601)
else()
    target_compile_options(helios_mirror_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    add_library(etherdream_bridge SHARED
        src/etherdream_proxy.cpp
        etherdream.def)
    target_link_libraries(etherdream_bridge PRIVATE helios_mirror_core)
    target_compile_features(etherdream_bridge PRIVATE cxx_std_17)
    set_target_properties(etherdream_bridge PROPERTIES OUTPUT_NAME EtherDream)

    add_executable(helios_mirror_controller WIN32 src/controller.cpp)
    target_compile_features(helios_mirror_controller PRIVATE cxx_std_17)
    set_target_properties(helios_mirror_controller PROPERTIES
        OUTPUT_NAME "TROLLfx-Helios-Mirror-V2")
endif()

if(BUILD_TESTING)
    add_executable(helios_mirror_core_tests tests/core_tests.cpp)
    target_link_libraries(helios_mirror_core_tests PRIVATE helios_mirror_core)
    add_test(NAME core COMMAND helios_mirror_core_tests)

    if(WIN32)
        add_library(mock_helios SHARED tests/mock_helios.cpp)
        target_include_directories(mock_helios PRIVATE include)
        set_target_properties(mock_helios PROPERTIES
            OUTPUT_NAME HeliosLaserDAC
            RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:etherdream_bridge>/abi")

        add_executable(etherdream_abi_smoke tests/abi_smoke.cpp)
        target_include_directories(etherdream_abi_smoke PRIVATE include)
        set_target_properties(etherdream_abi_smoke PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:etherdream_bridge>/abi")
        add_custom_command(TARGET etherdream_abi_smoke POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:etherdream_bridge>"
                "$<TARGET_FILE_DIR:etherdream_abi_smoke>/EtherDream.dll")
        add_dependencies(etherdream_abi_smoke mock_helios etherdream_bridge)
        add_test(NAME etherdream_abi COMMAND etherdream_abi_smoke)
        set_tests_properties(etherdream_abi PROPERTIES
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:etherdream_abi_smoke>")
    endif()
endif()
