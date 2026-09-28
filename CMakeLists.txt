cmake_minimum_required(VERSION 3.16)

project(seq
    VERSION     2.0.1
    LANGUAGES   C
    DESCRIPTION "Non blocking state sequencer and interrupt task queue for STM32"
    HOMEPAGE_URL "https://github.com/nimaltd/seq")

option(SEQ_BUILD_TESTS "Build the host unit tests" OFF)

# seq.c needs one header it does not ship: the platform main.h. Point
# SEQ_CONFIG_DIR at the folder holding it, normally Core/Inc in a CubeMX
# project. The tests supply a stub instead.
if(SEQ_BUILD_TESTS AND NOT DEFINED SEQ_CONFIG_DIR)
    set(SEQ_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test/stub")
endif()

add_library(seq STATIC src/seq.c)
add_library(nimaltd::seq ALIAS seq)

target_include_directories(seq PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

target_compile_features(seq PUBLIC c_std_11)

if(DEFINED SEQ_CONFIG_DIR)
    target_include_directories(seq PRIVATE ${SEQ_CONFIG_DIR})
endif()

if(SEQ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
