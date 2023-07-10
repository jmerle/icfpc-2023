cmake_minimum_required(VERSION 3.20)
project(icfpc-2023)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -march=native")

if (NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
    message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/0.18.1/conan.cmake"
            "${CMAKE_BINARY_DIR}/conan.cmake"
            TLS_VERIFY ON)
endif ()

include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_cmake_run(REQUIRES
        rapidjson/1.1.0
        cpp-httplib/0.12.5
        onetbb/2021.9.0
        gtest/1.13.0
        benchmark/1.8.0

        OPTIONS cpp-httplib:with_openssl=True onetbb:tbbmalloc=False

        BASIC_SETUP CMAKE_TARGETS
        BUILD missing)

add_custom_command(OUTPUT source.o dummy_target_run_always.h
        COMMAND fd --base-directory ${CMAKE_CURRENT_LIST_DIR} -H -E .git/ -t f -X zip -FS ${CMAKE_CURRENT_BINARY_DIR}/source.zip && ld -r -b binary -o source.o source.zip
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(dummy_target_run_always ALL DEPENDS dummy_target_run_always.h)

add_library(source STATIC source.o)
set_source_files_properties(source.o PROPERTIES EXTERNAL_OBJECT true GENERATED true)
set_target_properties(source PROPERTIES LINKER_LANGUAGE C)

set(CORE_LIBRARIES CONAN_PKG::rapidjson CONAN_PKG::cpp-httplib CONAN_PKG::onetbb source)
set(CORE_INCLUDES src)

add_executable(tests src/tests/tests.cpp)
target_link_libraries(tests PRIVATE ${CORE_LIBRARIES} CONAN_PKG::gtest)
target_include_directories(tests PRIVATE ${CORE_INCLUDES})

add_executable(benchmark src/tests/benchmark.cpp)
target_link_libraries(benchmark PRIVATE ${CORE_LIBRARIES} CONAN_PKG::benchmark)
target_include_directories(benchmark PRIVATE ${CORE_INCLUDES})

add_executable(starter src/solvers/starter.cpp)
target_link_libraries(starter PRIVATE ${CORE_LIBRARIES})
target_include_directories(starter PRIVATE ${CORE_INCLUDES})

add_executable(brute src/solvers/brute.cpp)
target_link_libraries(brute PRIVATE ${CORE_LIBRARIES})
target_include_directories(brute PRIVATE ${CORE_INCLUDES})
